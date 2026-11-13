#include <catch2/catch_test_macros.hpp>

#include <local_forwarder.h>

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <memory>
#include <string>
#include <thread>
#include <utility>

using tunnelpp::Message;
using tunnelpp::MessageType;
using tunnelpp::Url;
using tunnelpp::client::forward_to_local;

namespace
{
    class EchoServer
    {
    public:
        EchoServer()
        {
            const auto handler{ [](const httplib::Request& req, httplib::Response& res) {
                const auto body = nlohmann::json{
                    { "method", req.method },
                    { "path", req.target },
                    { "body", req.body },
                    { "x_test", req.get_header_value("X-Test") },
                };
                res.set_content(body.dump(), "application/json");
            } };
            _server.Get(R"((.*))", handler);
            _server.Post(R"((.*))", handler);
            _server.Put(R"((.*))", handler);
            _server.Delete(R"((.*))", handler);
            _port = _server.bind_to_any_port("127.0.0.1");
            _thread = std::thread([this] { _server.listen_after_bind(); });
        }

        ~EchoServer()
        {
            _server.stop();
            _thread.join();
        }

        int port() const
        {
            return _port;
        }

    private:
        httplib::Server _server;
        std::thread _thread;
        int _port{ 0 };
    };

    Url local_url(int port)
    {
        Url url;
        url.scheme = "http";
        url.host = "127.0.0.1";
        url.port = port;
        url.path = "/";
        return url;
    }

    Message make_request(const std::string& method, const std::string& path, const std::string& body = "")
    {
        Message request;
        request.type = MessageType::Request;
        request.id = "id-1";
        request.method = method;
        request.path = path;
        request.body = body;
        return request;
    }

    using BioPtr = std::unique_ptr<BIO, decltype(&BIO_free)>;
    using X509Ptr = std::unique_ptr<X509, decltype(&X509_free)>;
    using X509ExtPtr = std::unique_ptr<X509_EXTENSION, decltype(&X509_EXTENSION_free)>;
    using PkeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;

    std::string pem_to_string(BIO* bio)
    {
        const char* data{ nullptr };
        const long length{ BIO_get_mem_data(bio, &data) };
        return { data, static_cast<std::size_t>(length) };
    }

    // dev servers usually serve https with a self-signed certificate
    std::pair<std::string, std::string> self_signed_pem()
    {
        constexpr std::size_t rsa_bits{ 2048 };
        constexpr long one_day_seconds{ 24L * 60L * 60L };
        PkeyPtr pkey{ EVP_RSA_gen(rsa_bits), &EVP_PKEY_free };
        X509Ptr x509{ X509_new(), &X509_free };
        ASN1_INTEGER_set(X509_get_serialNumber(x509.get()), 1);
        X509_gmtime_adj(X509_getm_notBefore(x509.get()), 0);
        X509_gmtime_adj(X509_getm_notAfter(x509.get()), one_day_seconds);
        X509_set_pubkey(x509.get(), pkey.get());
        X509_NAME* name{ X509_get_subject_name(x509.get()) };
        const std::string common_name{ "localhost" };
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(common_name.data()), -1, -1, 0);
        X509_set_issuer_name(x509.get(), name);
        X509ExtPtr san{ X509V3_EXT_conf_nid(nullptr, nullptr, NID_subject_alt_name, "IP:127.0.0.1,DNS:localhost"), &X509_EXTENSION_free };
        X509_add_ext(x509.get(), san.get(), -1);
        X509_sign(x509.get(), pkey.get(), EVP_sha256());

        BioPtr cert_bio{ BIO_new(BIO_s_mem()), &BIO_free };
        PEM_write_bio_X509(cert_bio.get(), x509.get());
        BioPtr key_bio{ BIO_new(BIO_s_mem()), &BIO_free };
        PEM_write_bio_PrivateKey(key_bio.get(), pkey.get(), nullptr, nullptr, 0, nullptr, nullptr);
        return { pem_to_string(cert_bio.get()), pem_to_string(key_bio.get()) };
    }

    class EchoTlsServer
    {
    public:
        EchoTlsServer()
        {
            const auto [cert_pem, key_pem]{ self_signed_pem() };
            const httplib::SSLServer::PemMemory pem{
                .cert_pem = cert_pem.c_str(),
                .cert_pem_len = cert_pem.size(),
                .key_pem = key_pem.c_str(),
                .key_pem_len = key_pem.size(),
                .client_ca_pem = nullptr,
                .client_ca_pem_len = 0,
                .private_key_password = nullptr,
            };
            _server = std::make_unique<httplib::SSLServer>(pem);
            _server->Get(R"((.*))", [](const httplib::Request& req, httplib::Response& res) {
                res.set_content(nlohmann::json{ { "method", req.method }, { "path", req.target } }.dump(), "application/json");
            });
            _port = _server->bind_to_any_port("127.0.0.1");
            _thread = std::thread([this] { _server->listen_after_bind(); });
        }

        ~EchoTlsServer()
        {
            _server->stop();
            _thread.join();
        }

        [[nodiscard]] int port() const
        {
            return _port;
        }

    private:
        std::unique_ptr<httplib::SSLServer> _server;
        std::thread _thread;
        int _port{ 0 };
    };
}

TEST_CASE("forward_to_local get")
{
    EchoServer echo;
    auto request{ make_request("GET", "/hello?x=1") };
    request.headers.emplace("X-Test", "yes");
    const auto response{ forward_to_local(local_url(echo.port()), request, 5) };
    REQUIRE(response.type == MessageType::Response);
    REQUIRE(response.id == "id-1");
    REQUIRE(response.status == 200);
    const auto body = nlohmann::json::parse(response.body);
    REQUIRE(body.at("method") == "GET");
    REQUIRE(body.at("path") == "/hello?x=1");
    REQUIRE(body.at("x_test") == "yes");
}

TEST_CASE("forward_to_local post with body")
{
    EchoServer echo;
    const auto response{ forward_to_local(local_url(echo.port()), make_request("POST", "/api", "hello body"), 5) };
    REQUIRE(response.status == 200);
    const auto body = nlohmann::json::parse(response.body);
    REQUIRE(body.at("method") == "POST");
    REQUIRE(body.at("body") == "hello body");
}

TEST_CASE("forward_to_local connection refused")
{
    httplib::Server probe;
    const int free_port{ probe.bind_to_any_port("127.0.0.1") };
    probe.stop();
    const auto response{ forward_to_local(local_url(free_port), make_request("GET", "/"), 2) };
    REQUIRE(response.status == 502);
    REQUIRE(response.headers.count("Content-Type") == 1);
}

TEST_CASE("forward_to_local sends the request target bytes unchanged")
{
    EchoServer echo;
    // the tunnel server forwards req.target as-is, already percent-encoded by the browser:
    // '+' must stay '+', '%20' must not be double-encoded
    const auto response{ forward_to_local(local_url(echo.port()), make_request("GET", "/@fs/C:/a+b/c%20d.css?x=1+2"), 5) };
    REQUIRE(response.status == 200);
    REQUIRE(nlohmann::json::parse(response.body).at("path") == "/@fs/C:/a+b/c%20d.css?x=1+2");
}

TEST_CASE("forward_to_local https with self-signed certificate")
{
    EchoTlsServer echo;
    Url url;
    url.scheme = "https";
    url.host = "127.0.0.1";
    url.port = echo.port();
    url.path = "/";
    const auto response{ forward_to_local(url, make_request("GET", "/secure"), 5) };
    REQUIRE(response.status == 200);
    REQUIRE(nlohmann::json::parse(response.body).at("path") == "/secure");
}
