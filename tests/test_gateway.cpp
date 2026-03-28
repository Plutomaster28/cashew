#include "cashew/gateway/gateway_server.hpp"
#include "cashew/gateway/websocket_handler.hpp"
#include "cashew/gateway/content_renderer.hpp"
#include "cashew/common.hpp"
#include "crypto/blake3.hpp"
#include <gtest/gtest.h>

using namespace cashew;
using namespace cashew::gateway;

namespace {

Hash256 hash_of(const std::vector<uint8_t>& data) {
    return crypto::Blake3::hash(data);
}

} // namespace

TEST(GatewayTest, ContentTypeDetectionByMagicAndExtension) {
    const std::vector<uint8_t> png = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A};
    EXPECT_EQ(ContentRenderer::detect_content_type(png), ContentType::IMAGE_PNG);

    const std::vector<uint8_t> html = {'<', 'h', 't', 'm', 'l', '>'};
    EXPECT_EQ(ContentRenderer::detect_content_type(html, std::string("index.html")), ContentType::HTML);
    EXPECT_EQ(ContentRenderer::get_mime_type(ContentType::HTML), "text/html; charset=utf-8");
}

TEST(GatewayTest, RendererFetchesCachesAndServesRanges) {
    ContentRendererConfig cfg;
    cfg.sanitize_html = false;
    cfg.chunk_size = 4;

    ContentRenderer renderer(cfg);

    const std::vector<uint8_t> payload = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
    const Hash256 content_hash = hash_of(payload);

    renderer.set_fetch_callback([&](const Hash256& requested) -> std::optional<std::vector<uint8_t>> {
        if (requested == content_hash) {
            return payload;
        }
        return std::nullopt;
    });

    auto full = renderer.render_content(content_hash);
    ASSERT_TRUE(full.has_value());
    EXPECT_EQ(full->data, payload);
    EXPECT_TRUE(renderer.is_cached(content_hash));

    auto ranged = renderer.render_content(content_hash, std::make_pair<size_t, size_t>(2, 5));
    ASSERT_TRUE(ranged.has_value());
    EXPECT_TRUE(ranged->is_partial);
    EXPECT_EQ(ranged->data.size(), 4u);
    EXPECT_EQ(ranged->data[0], 'c');
    EXPECT_EQ(ranged->data[3], 'f');

    auto stats = renderer.get_cache_stats();
    EXPECT_GE(stats.hit_count, 1u);
    EXPECT_GE(stats.miss_count, 1u);
}

TEST(GatewayTest, RendererStreamsChunks) {
    ContentRendererConfig cfg;
    cfg.chunk_size = 3;
    cfg.sanitize_html = false;
    ContentRenderer renderer(cfg);

    const std::vector<uint8_t> payload = {'1', '2', '3', '4', '5', '6', '7'};
    const Hash256 content_hash = hash_of(payload);

    renderer.set_fetch_callback([&](const Hash256& requested) -> std::optional<std::vector<uint8_t>> {
        return requested == content_hash ? std::optional<std::vector<uint8_t>>(payload) : std::nullopt;
    });

    size_t chunks = 0;
    size_t total = 0;
    bool saw_final = false;

    ASSERT_TRUE(renderer.stream_content(content_hash, [&](const ContentChunk& c) {
        chunks++;
        total += c.length;
        if (c.is_final) {
            saw_final = true;
        }
    }));

    EXPECT_EQ(total, payload.size());
    EXPECT_TRUE(saw_final);
    EXPECT_EQ(chunks, 3u);
}

TEST(GatewayTest, WebSocketSubscriptionTrackingAndEventMapping) {
    WsHandlerConfig cfg;
    cfg.max_connections = 4;
    WebSocketHandler handler(cfg);

    auto c1 = handler.accept_connection("conn-1");
    auto c2 = handler.accept_connection("conn-2");
    ASSERT_NE(c1, nullptr);
    ASSERT_NE(c2, nullptr);

    handler.subscribe(c1, WsEventType::LEDGER_EVENT);
    handler.subscribe(c2, WsEventType::LEDGER_EVENT);

    auto stats = handler.get_statistics();
    EXPECT_EQ(stats.active_connections, 2u);
    EXPECT_EQ(stats.subscription_count, 2u);

    handler.unsubscribe(c1, WsEventType::LEDGER_EVENT);
    stats = handler.get_statistics();
    EXPECT_EQ(stats.subscription_count, 1u);

    EXPECT_EQ(parse_event_type(event_type_to_string(WsEventType::PEER_STATUS)), WsEventType::PEER_STATUS);
}

TEST(GatewayTest, GatewayServerConstructsAndRegistersHandlers) {
    GatewayConfig config;
    config.bind_address = "127.0.0.1";
    config.http_port = 18080;
    GatewayServer server(config);

    EXPECT_FALSE(server.is_running());

    server.register_handler(HttpMethod::GET, "/api/test", [](const HttpRequest&, GatewaySession&) {
        HttpResponse response;
        response.status = HttpStatus::OK;
        response.set_json_body("{\"ok\":true}");
        return response;
    });

    const auto stats = server.get_statistics();
    EXPECT_EQ(stats.total_requests, 0u);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
