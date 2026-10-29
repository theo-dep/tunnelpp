#include <catch2/catch_test_macros.hpp>

#include <pending.h>

using tunnelpp::Message;
using tunnelpp::MessageType;
using tunnelpp::server::PendingRequestTable;

namespace
{
    Message make_response(const std::string& id, int status)
    {
        Message response;
        response.type = MessageType::Response;
        response.id = id;
        response.status = status;
        return response;
    }
}

TEST_CASE("PendingRequestTable complete")
{
    PendingRequestTable table;
    const auto request{ table.add("id-1", nullptr) };
    REQUIRE(table.size() == 1);
    table.complete("id-1", make_response("id-1", 200));
    Message out;
    REQUIRE(table.wait(request, 1, out) == PendingRequestTable::WaitOutcome::Ok);
    REQUIRE(out.status == 200);
    REQUIRE(table.size() == 0);
}

TEST_CASE("PendingRequestTable wait timeout")
{
    PendingRequestTable table;
    const auto request{ table.add("id-1", nullptr) };
    Message out;
    REQUIRE(table.wait(request, 1, out) == PendingRequestTable::WaitOutcome::Timeout);
    REQUIRE(table.size() == 0);
    table.complete("id-1", make_response("id-1", 200)); // late response is dropped
    REQUIRE(table.size() == 0);
}

TEST_CASE("PendingRequestTable fail_all_for")
{
    PendingRequestTable table;
    int ws_a{ 0 };
    int ws_b{ 0 };
    auto* a{ reinterpret_cast<httplib::ws::WebSocket*>(&ws_a) };
    auto* b{ reinterpret_cast<httplib::ws::WebSocket*>(&ws_b) };
    const auto request_a1{ table.add("a-1", a) };
    const auto request_a2{ table.add("a-2", a) };
    const auto request_b{ table.add("b-1", b) };
    table.fail_all_for(a);
    Message out;
    REQUIRE(table.wait(request_a1, 1, out) == PendingRequestTable::WaitOutcome::Failed);
    REQUIRE(table.wait(request_a2, 1, out) == PendingRequestTable::WaitOutcome::Failed);
    table.complete("b-1", make_response("b-1", 204));
    REQUIRE(table.wait(request_b, 1, out) == PendingRequestTable::WaitOutcome::Ok);
    REQUIRE(out.status == 204);
}

TEST_CASE("PendingRequestTable complete unknown id is dropped")
{
    PendingRequestTable table;
    table.complete("nope", make_response("nope", 200));
    REQUIRE(table.size() == 0);
}

TEST_CASE("PendingRequestTable remove")
{
    PendingRequestTable table;
    const auto request{ table.add("id-1", nullptr) };
    table.remove("id-1");
    REQUIRE(table.size() == 0);
    Message out;
    REQUIRE(table.wait(request, 1, out) == PendingRequestTable::WaitOutcome::Failed);
}
