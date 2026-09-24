#include "provider_adapters.h"

#include <map>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "coach_response_json.h"
#include "provider_prompt.h"
#include "provider_transport.h"

namespace kchess::ai {
namespace {

using json = nlohmann::json;

// -----------------------------------------------------------------------------
// Section: Request serialization
// -----------------------------------------------------------------------------

// Claude structured outputs require every object schema to forbid additional
// properties.
void close_object_schemas(json& schema) {
  if (schema.is_object()) {
    if (schema.contains("type") && schema["type"] == "object") {
      schema["additionalProperties"] = false;
    }
    for (auto& child : schema) close_object_schemas(child);
  } else if (schema.is_array()) {
    for (auto& child : schema) close_object_schemas(child);
  }
}

json request_body(const LLMProviderRequest& request, const ProviderConfig& config) {
  json schema = coach_response_json::schema();
  close_object_schemas(schema);
  json output_config = {
      {"format", {{"type", "json_schema"}, {"schema", std::move(schema)}}}};
  if (!config.effort.empty()) output_config["effort"] = config.effort;

  json message = {{"role", "user"}, {"content", coach_provider_input(request)}};
  json messages = json::array();
  messages.push_back(std::move(message));

  json body = {
      {"model", config.model},
      {"max_tokens", config.max_output_tokens},
      {"system", coach_system_instruction(request)},
      {"messages", std::move(messages)},
      {"output_config", std::move(output_config)},
  };
  if (config.refusal_fallback) body["fallbacks"] = "default";
  return body;
}

// -----------------------------------------------------------------------------
// Section: Response parsing
// -----------------------------------------------------------------------------

bool refused(const std::string& body) {
  try {
    const auto root = json::parse(body);
    return root.contains("stop_reason") && root["stop_reason"] == "refusal";
  } catch (...) {
    return false;
  }
}

// Truncated output (stop_reason "max_tokens") fails JSON parsing and is
// reported as an invalid response.
StructuredCoachContent parse_response(const std::string& body) {
  const auto root = json::parse(body);
  if (!root.contains("content") || !root["content"].is_array()) return {};
  for (const auto& block : root["content"]) {
    if (!block.is_object() || block.value("type", "") != "text") continue;
    const auto text = block.value("text", "");
    if (text.empty()) continue;
    const auto structured = json::parse(text);
    if (structured.contains("answer_segments") &&
        structured["answer_segments"].is_array()) {
      return coach_response_json::parse(structured);
    }
  }
  return {};
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Claude transport
// -----------------------------------------------------------------------------

LLMProviderResult complete_claude(const ProviderConfig& config,
                                  const LLMProviderRequest& request,
                                  const RemoteProviderConfig& remote) {
  std::map<std::string, std::string> headers = {
      {"x-api-key", config.api_key},
      {"anthropic-version", "2023-06-01"},
  };
  if (config.refusal_fallback) {
    headers["anthropic-beta"] = "server-side-fallback-2026-07-01";
  }
  LLMProviderResult failure;
  const auto body = post_provider_request(config, request, remote,
                                          request_body(request, config).dump(),
                                          std::move(headers), failure);
  if (!body) return failure;
  if (refused(*body)) {
    return provider_failure(config, remote, LLMProviderStatus::error,
                            config.id + "_refusal");
  }
  return provider_content_result(config, remote, *body, parse_response);
}

}  // namespace kchess::ai
