# TLS/HTTPS Guide

This guide covers TLS/SSL configuration for both HTTPS clients (making requests) and HTTPS servers in ICICLE Insights.

## Table of Contents

- [Client-Side HTTPS (Making Requests)](#client-side-https-making-requests)
- [Server-Side HTTPS (Serving Content)](#server-side-https-serving-content)
- [Certificate Management](#certificate-management)
- [Troubleshooting](#troubleshooting)

---

## Client-Side HTTPS (Making Requests)

### Prerequisites

**macOS (Homebrew):**
```bash
brew install ca-certificates openssl@3
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install ca-certificates openssl
```

The `ca-certificates` package provides the trusted root certificate bundle needed to verify SSL/TLS connections.

### Basic HTTPS Client Configuration

When using `glz::http_client` to make HTTPS requests, you need to configure the SSL context.

**Using Config (Recommended):**

```cpp
#include <glaze/net/http_client.hpp>
#include <asio/ssl.hpp>
#include <spdlog/spdlog.h>
#include <system_error>
#include "core/config.hpp"

// Load config with SSL_CERT_FILE from environment
auto Config = insights::core::Config::load();

glz::http_client Client;

// Configure SSL to use system certificates
Client.configure_ssl_context([&Config](auto& Ctx) {
    std::error_code Ec;

    // Use SSL_CERT_FILE from config if set
    if (Config->SslCertFile) {
        Ctx.load_verify_file(*Config->SslCertFile, Ec);
        if (Ec) {
            spdlog::warn("Failed to load CA certificates from SSL_CERT_FILE ({}): {}",
                         *Config->SslCertFile, Ec.message());
        }
    }

    // Fall back to default paths if not set or failed
    if (!Config->SslCertFile || Ec) {
        Ctx.set_default_verify_paths(Ec);
        if (Ec) {
            spdlog::warn("Failed to set default verify paths: {}", Ec.message());
        }
    }

    // Enable certificate verification (recommended for production)
    Ctx.set_verify_mode(asio::ssl::verify_peer);
});

// Make HTTPS request
auto Response = Client.get("https://api.github.com/repos/foo/bar", Headers);
```

**Direct Hardcoded Path (Not Recommended):**

```cpp
glz::http_client Client;

Client.configure_ssl_context([](auto& Ctx) {
    std::error_code Ec;
    Ctx.load_verify_file("/opt/homebrew/etc/ca-certificates/cert.pem", Ec);
    if (Ec) {
        Ctx.set_default_verify_paths(Ec);
    }
    Ctx.set_verify_mode(asio::ssl::verify_peer);
});
```

### Configuration via Environment Variable

The recommended way to configure certificate paths is via the `SSL_CERT_FILE` environment variable:

```bash
# In .env file
SSL_CERT_FILE=/opt/homebrew/etc/ca-certificates/cert.pem
```

Or export it before running:

```bash
export SSL_CERT_FILE=/opt/homebrew/etc/ca-certificates/cert.pem
just run
```

The application will automatically load this from `Config.SslCertFile` and use it for HTTPS requests.

### Certificate Bundle Locations by Platform

| Platform | Certificate Path |
|----------|-----------------|
| macOS (Homebrew) | `/opt/homebrew/etc/ca-certificates/cert.pem` |
| macOS (Intel) | `/usr/local/etc/ca-certificates/cert.pem` |
| Ubuntu/Debian | `/etc/ssl/certs/ca-certificates.crt` |
| RHEL/CentOS | `/etc/pki/tls/certs/ca-bundle.crt` |
| Alpine Linux | `/etc/ssl/certs/ca-certificates.crt` |

### Multi-Platform Certificate Loading

For code that runs on multiple platforms:

```cpp
Client.configure_ssl_context([](auto& Ctx) {
    std::error_code Ec;

    // Try platform-specific paths
    const std::vector<std::string> CertPaths = {
        "/opt/homebrew/etc/ca-certificates/cert.pem",  // macOS (Apple Silicon)
        "/usr/local/etc/ca-certificates/cert.pem",     // macOS (Intel)
        "/etc/ssl/certs/ca-certificates.crt",          // Ubuntu/Debian/Alpine
        "/etc/pki/tls/certs/ca-bundle.crt",            // RHEL/CentOS
    };

    bool Loaded = false;
    for (const auto& Path : CertPaths) {
        Ctx.load_verify_file(Path, Ec);
        if (!Ec) {
            Loaded = true;
            break;
        }
    }

    if (!Loaded) {
        spdlog::warn("No certificate bundle found, trying default paths");
        Ctx.set_default_verify_paths(Ec);
    }

    Ctx.set_verify_mode(asio::ssl::verify_peer);
});
```

### Development vs Production

**Production (Recommended):**
```cpp
Ctx.set_verify_mode(asio::ssl::verify_peer);  // Verify certificates
```

**Development Only (NOT SECURE):**
```cpp
// WARNING: Only for testing with self-signed certificates
// This makes you vulnerable to man-in-the-middle attacks
Ctx.set_verify_mode(asio::ssl::verify_none);
```

### Using HTTPS

Once configured, HTTPS works automatically:

```cpp
// HTTP request
auto Response = Client.get("http://example.com/api", Headers);

// HTTPS request (uses SSL/TLS automatically)
auto Response = Client.get("https://example.com/api", Headers);
```

The client detects the `https://` protocol and handles:
- SSL socket creation
- TLS handshake with SNI (Server Name Indication)
- Certificate verification
- TLS 1.2/1.3 negotiation

---

## Server-Side HTTPS (Serving Content)

### Prerequisites

You need an SSL certificate and private key. For development:

```bash
# Generate self-signed certificate (development only)
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes \
  -subj "/CN=localhost"
```

For production, use certificates from:
- [Let's Encrypt](https://letsencrypt.org/) (free, automated)
- Commercial CA (DigiCert, GlobalSign, etc.)
- Internal CA for private networks

### Configuring HTTPS Server

```cpp
#include <glaze/net/http_router.hpp>
#include <asio/ssl.hpp>

// Create SSL context for server
asio::ssl::context SslContext(asio::ssl::context::tlsv12_server);

// Load certificate and private key
SslContext.use_certificate_chain_file("path/to/cert.pem");
SslContext.use_private_key_file("path/to/key.pem", asio::ssl::context::pem);

// Optional: Configure cipher suites (for security)
SslContext.set_options(
    asio::ssl::context::default_workarounds |
    asio::ssl::context::no_sslv2 |
    asio::ssl::context::no_sslv3 |
    asio::ssl::context::single_dh_use
);

// Create HTTPS server with glaze
// TODO: Add glaze HTTPS server example when implemented
```

### Certificate Files

Two files are required:

1. **Certificate Chain (`cert.pem`)**: Your certificate + intermediate certificates
2. **Private Key (`key.pem`)**: Your private key (keep secure!)

**Important:** Never commit private keys to version control. Use environment variables or secure key management:

```cpp
auto CertPath = std::getenv("SSL_CERT_FILE");
auto KeyPath = std::getenv("SSL_KEY_FILE");

if (!CertPath || !KeyPath) {
    spdlog::error("SSL_CERT_FILE and SSL_KEY_FILE must be set");
    return 1;
}

SslContext.use_certificate_chain_file(CertPath);
SslContext.use_private_key_file(KeyPath, asio::ssl::context::pem);
```

### Environment Configuration

Add to `.env`:

```bash
# HTTPS Server Configuration
SSL_CERT_FILE=/path/to/cert.pem
SSL_KEY_FILE=/path/to/key.pem

# Client Configuration (if needed)
SSL_CERT_DIR=/etc/ssl/certs
```

---

## Certificate Management

### Obtaining Production Certificates

**Let's Encrypt (Recommended for Production):**

```bash
# Install certbot
brew install certbot  # macOS
sudo apt-get install certbot  # Ubuntu

# Obtain certificate
sudo certbot certonly --standalone -d yourdomain.com

# Certificates will be in:
# /etc/letsencrypt/live/yourdomain.com/fullchain.pem  (certificate)
# /etc/letsencrypt/live/yourdomain.com/privkey.pem    (private key)
```

### Certificate Renewal

Let's Encrypt certificates expire every 90 days. Set up auto-renewal:

```bash
# Test renewal
sudo certbot renew --dry-run

# Add to crontab for automatic renewal
0 0 * * * certbot renew --quiet && systemctl reload your-service
```

### Certificate Validation

Verify your certificate setup:

```bash
# Check certificate expiration
openssl x509 -in cert.pem -noout -dates

# Verify certificate chain
openssl verify -CAfile /opt/homebrew/etc/ca-certificates/cert.pem cert.pem

# Test HTTPS connection
curl -v https://localhost:3000
```

---

## Troubleshooting

### "certificate verify failed"

**Cause:** OpenSSL can't find or verify the CA certificate bundle.

**Solutions:**

1. **Install CA certificates:**
   ```bash
   brew install ca-certificates  # macOS
   sudo apt-get install ca-certificates  # Linux
   ```

2. **Explicitly load certificate file:**
   ```cpp
   Ctx.load_verify_file("/opt/homebrew/etc/ca-certificates/cert.pem");
   ```

3. **Set environment variable:**
   ```bash
   export SSL_CERT_FILE=/opt/homebrew/etc/ca-certificates/cert.pem
   ```

4. **Check file exists:**
   ```bash
   ls -la /opt/homebrew/etc/ca-certificates/cert.pem
   ```

### "SSL routines: OPENSSL_internal: WRONG_VERSION_NUMBER"

**Cause:** Client is trying SSL/TLS on a non-HTTPS port, or server is not configured for HTTPS.

**Solution:**
- Ensure URL uses `https://` not `http://`
- Verify server is listening on HTTPS port

### "handshake failed"

**Cause:** TLS version mismatch or cipher suite incompatibility.

**Solutions:**

1. **Use modern TLS versions:**
   ```cpp
   asio::ssl::context SslContext(asio::ssl::context::tlsv12_server);  // TLS 1.2+
   ```

2. **Check supported ciphers:**
   ```bash
   openssl ciphers -v
   ```

### "certificate has expired"

**Cause:** Certificate validity period has passed.

**Solution:**
- Renew certificate with Let's Encrypt: `certbot renew`
- Replace with new certificate from CA

### Debugging SSL Issues

Enable verbose logging:

```cpp
// In client code
spdlog::set_level(spdlog::level::debug);

Client.configure_ssl_context([](auto& Ctx) {
    std::error_code Ec;
    Ctx.load_verify_file(CertPath, Ec);
    if (Ec) {
        spdlog::error("Failed to load certs: {} ({})", Ec.message(), Ec.value());
    } else {
        spdlog::debug("Successfully loaded certificates from {}", CertPath);
    }
});
```

Test with OpenSSL command line:

```bash
# Test connection to server
openssl s_client -connect api.github.com:443 -servername api.github.com

# Check what certificates are sent
openssl s_client -connect api.github.com:443 -showcerts
```

---

## Security Best Practices

### For Clients

✅ **Do:**
- Always use `verify_peer` in production
- Keep CA certificate bundle updated
- Use TLS 1.2 or higher
- Validate hostnames (SNI)

❌ **Don't:**
- Use `verify_none` in production
- Disable certificate verification
- Use outdated TLS versions (SSLv3, TLS 1.0, TLS 1.1)

### For Servers

✅ **Do:**
- Use certificates from trusted CAs in production
- Rotate certificates before expiration
- Use strong cipher suites
- Enable HSTS (HTTP Strict Transport Security)
- Keep private keys secure (never commit to git)

❌ **Don't:**
- Use self-signed certificates in production
- Share private keys
- Use weak ciphers (RC4, DES, MD5)
- Expose private keys in logs or error messages

### Example Secure Configuration

```cpp
// Client (production)
Client.configure_ssl_context([](auto& Ctx) {
    Ctx.load_verify_file("/opt/homebrew/etc/ca-certificates/cert.pem");
    Ctx.set_verify_mode(asio::ssl::verify_peer);

    // Use modern TLS versions only
    Ctx.set_options(
        asio::ssl::context::no_sslv2 |
        asio::ssl::context::no_sslv3 |
        asio::ssl::context::no_tlsv1 |
        asio::ssl::context::no_tlsv1_1
    );
});

// Server (production)
asio::ssl::context ServerCtx(asio::ssl::context::tlsv12_server);
ServerCtx.use_certificate_chain_file(std::getenv("SSL_CERT_FILE"));
ServerCtx.use_private_key_file(std::getenv("SSL_KEY_FILE"), asio::ssl::context::pem);
ServerCtx.set_options(
    asio::ssl::context::default_workarounds |
    asio::ssl::context::no_sslv2 |
    asio::ssl::context::no_sslv3 |
    asio::ssl::context::single_dh_use
);
```

---

## References

- [OpenSSL Documentation](https://www.openssl.org/docs/)
- [Asio SSL Documentation](https://think-async.com/Asio/asio-1.28.0/doc/asio/overview/ssl.html)
- [Let's Encrypt](https://letsencrypt.org/)
- [Mozilla SSL Configuration Generator](https://ssl-config.mozilla.org/)
- [TLS Best Practices (RFC 9325)](https://datatracker.ietf.org/doc/html/rfc9325)
