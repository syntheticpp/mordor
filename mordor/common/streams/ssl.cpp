// Copyright (c) 2009 - Decho Corp.

#include "ssl.h"

#include "common/version.h"

#ifdef MSVC
#pragma comment(lib, "libeay32")
#pragma comment(lib, "ssleay32")
#endif

SSLStream::SSLStream(Stream::ptr parent, bool client, bool own)
: OpenSSLStream(BIO_new_stream(parent, own))
{
    assert(parent);
    m_ctx = SSL_CTX_new(client ? SSLv23_client_method() :
        SSLv23_server_method());
    assert(m_ctx);
    // TODO: exception
    m_ssl = SSL_new(m_ctx);
    // TODO: exception
    assert(m_ssl);
    SSL_set_bio(m_ssl, OpenSSLStream::parent(), OpenSSLStream::parent());
}

SSLStream::~SSLStream()
{
    SSL_free(m_ssl);
    SSL_CTX_free(m_ctx);
}

void
SSLStream::close(CloseType type)
{
    SSL_shutdown(m_ssl);
}

size_t
SSLStream::read(Buffer &b, size_t len)
{
    std::vector<iovec> bufs = b.writeBufs(len);
    int toRead = (int)std::min<size_t>(0x0fffffff, bufs[0].iov_len);
    int result = SSL_read(m_ssl, bufs[0].iov_base, toRead);
    if (result > 0) {
        b.produce(result);
        return result;
    }
    // TODO: exception
    return 0;
}

size_t
SSLStream::write(const Buffer &b, size_t len)
{
    std::vector<iovec> bufs = b.readBufs(len);
    int toWrite = (int)std::min<size_t>(0x0fffffff, bufs[0].iov_len);
    int result = SSL_write(m_ssl, bufs[0].iov_base, toWrite);
    if (result > 0) {
        return result;
    }
    // TODO: exception
    return 0;
}
