#include "provider_adapters.h"

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

json request_body(const LLMProviderRequest& request, const ProviderConfig& config) {
  json schema = coach_response_json::schema();
  json body = {
      {"model", config.model},
      {"store", false},
      {"system_instruction", coach_system_instruction(request)},
      {"input", coach_provider_input(request)},
      {"generation_config", {{"max_output_tokens", config.max_output_tokens}}},
      {"response_format",
       {{"type", "text"}, {"mime_type", "application/json"}, {"schema", std::move(schema)}}},
  };
  return body;
}

// -----------------------------------------------------------------------------
// Section: Response parsing
// -----------------------------------------------------------------------------

StructuredCoachContent parse_response(const std::string& body) {
  const auto root = json::parse(body);
  if (!root.contains("steps") || !root["steps"].is_array()) return {};
  for (auto step = root["steps"].rbegin(); step != root["steps"].rend(); ++step) {
    if (!step->is_object() || step->value("type", "") != "model_output") continue;
    if (!step->contains("content") || !(*step)["content"].is_array()) continue;
    for (auto content = (*step)["content"].rbegin();
         content != (*step)["content"].rend(); ++content) {
      if (!content->is_object() || content->value("type", "") != "text") continue;
      const auto text = content->value("text", "");
      if (text.empty()) continue;
      const auto structured = json::parse(text);
      if (structured.contains("answer_segments") &&
          structured["answer_segments"].is_array()) {
        return coach_response_json::parse(structured);
      }
    }
  }
  return {};
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Gemini transport
// -----------------------------------------------------------------------------

LLMProviderResult complete_gemini(const ProviderConfig& config,
                                  const LLMProviderRequest& request,
                                  const RemoteProviderConfig& remote) {
  LLMProviderResult failure;
  const auto body = post_provider_request(
      config, request, remote, request_body(request, config).dump(),
      {{"x-goog-api-key", config.api_key}}, failure);
  if (!body) return failure;
  return provider_content_result(config, remote, *body, parse_response);
}

}  // namespace kchess::ai
