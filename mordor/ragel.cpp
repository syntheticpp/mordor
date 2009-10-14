// Copyright (c) 2009 - Decho Corp.

#include "mordor/pch.h"

#include "ragel.h"

#include "assert.h"
#include "log.h"
#include "mordor/string.h"

namespace Mordor {

static Logger::ptr g_log = Log::lookup("mordor:ragel");

size_t
RagelParser::run(const void *buf, size_t len)
{
    init();
    p = (const char *)buf;
    pe = p + len;
    eof = pe;

    exec();

    MORDOR_ASSERT(!(final() && error()));
    return len - (pe - p);
}

size_t
RagelParser::run(const char *str)
{
    return run(str, strlen(str));
}

size_t
RagelParser::run(const std::string& str)
{
    return run(str.c_str(), str.length());
}

size_t
RagelParser::run(const Buffer& b)
{
    init();
    size_t total = 0;

    const std::vector<iovec> bufs = b.readBufs();
    for (size_t i = 0; i < bufs.size(); ++i) {
        size_t consumed = run(bufs[i].iov_base, bufs[i].iov_len, false);
        total += consumed;
        if (consumed < bufs[i].iov_len) {
            MORDOR_ASSERT(final() || error());
            return total;
        }
        if (error() || complete())
            return total;
    }
    run(NULL, 0, true);
    return total;
}

unsigned long long
RagelParser::run(Stream &stream)
{
    unsigned long long total = 0;
    init();
    Buffer b;
    bool inferredComplete = false;
    while (!error() && !complete() && !inferredComplete) {
        // TODO: limit total amount read
        size_t read = stream.read(b, 65536);
        if (read == 0) {
            run(NULL, 0, true);
            break;
        } else {
            const std::vector<iovec> bufs = b.readBufs();
            for (size_t i = 0; i < bufs.size(); ++i) {
                size_t consumed = run(bufs[i].iov_base, bufs[i].iov_len, false);
                total += consumed;
                b.consume(consumed);
                if (consumed < bufs[i].iov_len) {
                    MORDOR_ASSERT(final() || error());
                    inferredComplete = true;
                    break;
                }
                if (error() || complete())
                    break;
            }
        }
    }
    if (stream.supportsUnread()) {
        stream.unread(b, b.readAvailable());
    }
    return total;
}

void
RagelParser::init()
{
    mark = NULL;
    m_fullString.clear();
}

size_t
RagelParser::run(const void *buf, size_t len, bool isEof)
{
    MORDOR_ASSERT(!error());

    size_t markSpot = ~0;

    // Remember and reset mark in case fullString gets moved
    if (mark) {
        markSpot = mark - m_fullString.c_str();
        m_fullString.append((const char *)buf, len);

        if (markSpot != (size_t)~0) {
            mark = m_fullString.c_str() + markSpot;
        }
        p = m_fullString.c_str();
        pe = p + m_fullString.length();
        p = pe - len;
    } else {
        p = (const char *)buf;
        pe = p + len;
    }

    if (isEof) {
        eof = pe;
    } else {
        eof = NULL;
    }

    MORDOR_LOG_VERBOSE(g_log) << charslice(p, pe - p);
    exec();

    if (!mark) {
        m_fullString.clear();
    } else {
        if (m_fullString.empty()) {
            m_fullString.append(mark, pe - mark);
            mark = m_fullString.c_str();
        } else {
            markSpot = mark - m_fullString.c_str();
            m_fullString = m_fullString.substr(markSpot);
            mark = m_fullString.c_str();
        }
    }

    return p - (pe - len);
}

void
RagelParserWithStack::prepush()
{
    if (stack.empty())
        stack.resize(1);
    if (top >= stack.size())
        stack.resize(stack.size() * 2);
}

void
RagelParserWithStack::postpop()
{
    if (top <= stack.size() / 4)
        stack.resize(stack.size() / 2);
}

}