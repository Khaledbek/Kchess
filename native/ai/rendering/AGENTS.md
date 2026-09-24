# KChess native Coach rendering

`VerifiedFactRenderer` is the final native notation boundary for provider Coach output. It may resolve only opaque move/fact tokens that are present in exact native candidate evidence and authorized by the segment's verified Fact IDs or exact candidate-bound claim. It must never infer chess facts, choose moves, translate coaching prose, or accept provider-authored UCI notation. Rendering runs only after `ResponseValidator` succeeds.
