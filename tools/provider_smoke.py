"""Manual, non-CI smoke test for the public Phase-4 provider C ABI."""

from __future__ import annotations

import argparse
import ctypes
import json
import pathlib
import time


def configure(library: ctypes.CDLL) -> None:
    library.kc_core_create.argtypes = [ctypes.c_char_p]
    library.kc_core_create.restype = ctypes.c_void_p
    library.kc_core_destroy.argtypes = [ctypes.c_void_p]
    library.kc_core_initialize.argtypes = [ctypes.c_void_p]
    library.kc_core_initialize.restype = ctypes.c_int32
    library.kc_core_last_error.argtypes = [ctypes.c_void_p]
    library.kc_core_last_error.restype = ctypes.c_char_p
    library.kc_start_provider_profile_json.argtypes = [
        ctypes.c_void_p,
        ctypes.c_int32,
        ctypes.c_char_p,
    ]
    library.kc_start_provider_profile_json.restype = ctypes.c_void_p
    library.kc_provider_job_status_json.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    library.kc_provider_job_status_json.restype = ctypes.c_void_p
    library.kc_start_provider_sync_json.argtypes = [
        ctypes.c_void_p,
        ctypes.c_char_p,
        ctypes.c_int32,
        ctypes.c_int32,
    ]
    library.kc_start_provider_sync_json.restype = ctypes.c_void_p
    library.kc_games_json.argtypes = [ctypes.c_void_p]
    library.kc_games_json.restype = ctypes.c_void_p
    library.kc_string_free.argtypes = [ctypes.c_void_p]


def take_json(library: ctypes.CDLL, core: int, pointer: int | None) -> object:
    if not pointer:
        message = library.kc_core_last_error(core).decode("utf-8", errors="replace")
        raise RuntimeError(message)
    try:
        return json.loads(ctypes.string_at(pointer).decode("utf-8"))
    finally:
        library.kc_string_free(pointer)


def smoke(
    library: ctypes.CDLL,
    base: pathlib.Path,
    provider_type: int,
    provider_name: str,
    username: str,
) -> None:
    directory = base / provider_name
    directory.mkdir(parents=True, exist_ok=True)
    core = library.kc_core_create(str(directory).encode("utf-8"))
    if not core:
        raise RuntimeError("could not create native core")
    try:
        if library.kc_core_initialize(core) != 0:
            raise RuntimeError(library.kc_core_last_error(core).decode("utf-8"))
        started = take_json(
            library,
            core,
            library.kc_start_provider_profile_json(
                core, provider_type, username.encode("utf-8")
            ),
        )
        job_id = started["jobId"]
        deadline = time.monotonic() + 300
        while True:
            status = take_json(
                library,
                core,
                library.kc_provider_job_status_json(core, job_id.encode("utf-8")),
            )
            if status["finished"]:
                if status["state"] != "complete":
                    raise RuntimeError(
                        f"{status.get('errorKind')}: {status.get('errorMessage')}"
                    )
                break
            if time.monotonic() >= deadline:
                raise TimeoutError(f"{provider_name} smoke timed out")
            time.sleep(0.15)
        overview = status["result"]
        games = take_json(library, core, library.kc_games_json(core))
        for archive_month in overview["availableMonths"][:12] if not games else []:
            year, month = map(int, archive_month.split("-"))
            archive_started = take_json(
                library,
                core,
                library.kc_start_provider_sync_json(
                    core, overview["profile"]["id"].encode("utf-8"), year, month
                ),
            )
            archive_job_id = archive_started["jobId"]
            archive_deadline = time.monotonic() + 300
            while True:
                archive_status = take_json(
                    library,
                    core,
                    library.kc_provider_job_status_json(
                        core, archive_job_id.encode("utf-8")
                    ),
                )
                if archive_status["finished"]:
                    if archive_status["state"] != "complete" and archive_status.get(
                        "errorKind"
                    ) not in {"notFound", "gone"}:
                        raise RuntimeError(
                            f"{archive_status.get('errorKind')}: "
                            f"{archive_status.get('errorMessage')}"
                        )
                    break
                if time.monotonic() >= archive_deadline:
                    raise TimeoutError(f"{provider_name} archive smoke timed out")
                time.sleep(0.15)
            games = take_json(library, core, library.kc_games_json(core))
            if games:
                break
        if not games:
            raise RuntimeError(f"{provider_name} returned no completed current-month game")
        print(
            json.dumps(
                {
                    "provider": provider_name,
                    "username": overview["profile"]["providerUsername"],
                    "stats": len(overview["stats"]),
                    "games": len(games),
                    "firstGame": games[0]["providerGameId"],
                    "warning": status.get("errorKind"),
                },
                ensure_ascii=False,
            )
        )
    finally:
        library.kc_core_destroy(core)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", type=pathlib.Path, required=True)
    parser.add_argument("--data-dir", type=pathlib.Path, required=True)
    parser.add_argument("--chess-com-user", required=True)
    parser.add_argument("--lichess-user", required=True)
    args = parser.parse_args()
    library = ctypes.CDLL(str(args.library.resolve()))
    configure(library)
    smoke(library, args.data_dir, 0, "chess.com", args.chess_com_user)
    smoke(library, args.data_dir, 1, "lichess", args.lichess_user)


if __name__ == "__main__":
    main()
