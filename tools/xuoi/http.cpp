// MIT License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include "http.h"

#include <vector>
#include <string.h>
#include <assert.h>

#include <curl/curl.h>
#include <external/tinyxml2.h> // not really needed here, just for the version info

#define LOG_NEWLINE
#define LOG_DEBUG(...) // comment to enable debug logging
#define LOG_TRACE(...) // comment to enable tracing
#include <common/fs.h>
#include <common/log.h>

static const char *s_agentName = nullptr;

#define DO_CURL(x)                                                                                 \
    do                                                                                             \
    {                                                                                              \
        CURLcode r = curl_easy_##x;                                                                \
        if (r != CURLE_OK)                                                                         \
            LOG_ERROR(__FILE__ ":%d:ERROR:%d: %s", __LINE__, r, curl_easy_strerror(r));            \
    } while (0)

static size_t recv_data_string(const char *data, size_t size, size_t nmemb, std::string *str)
{
    assert(data && str && size && nmemb);
    const auto amount = size * nmemb;
    str->append(data, amount);
    return amount;
}

static size_t recv_data_vector(const char *data, size_t size, size_t nmemb, std::vector<char> *vec)
{
    assert(data && vec && size && nmemb);
    const auto amount = size * nmemb;
    vec->insert(vec->end(), data, data + amount);
    return amount;
}

struct http_recv_buf
{
    size_t max_len;
    size_t offset;
    const uint8_t *data;
};
static size_t recv_data(const char *data, size_t size, size_t nmemb, http_recv_buf *buf)
{
    assert(data && buf && size && nmemb);
    const auto amount = size * nmemb;
    assert(buf->offset + amount < buf->max_len);
    memcpy((void *)&buf->data[buf->offset], data, amount);
    buf->offset += amount;
    return amount;
}

static CURL *s_curl_handle = nullptr;
void http_init()
{
    if (s_curl_handle)
        return;
    curl_global_init(CURL_GLOBAL_ALL);
    curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
    LOG_INFO(
        "libcurl %s (%s, %s, %s, libz/%s, tinyxml2/%d.%d.%d)",
        info->version,
        info->host,
        info->ssl_version,
        info->libssh_version,
        info->libz_version,
        TINYXML2_MAJOR_VERSION,
        TINYXML2_MINOR_VERSION,
        TINYXML2_PATCH_VERSION);
    s_curl_handle = curl_easy_init();
}

void http_set_user_agent(const char *name)
{
    s_agentName = name;
}

void http_shutdown()
{
    assert(s_curl_handle && "http_init wasn't called");
    curl_easy_cleanup(s_curl_handle);
    s_curl_handle = nullptr;
    curl_global_cleanup();
}

void http_get_binary(const char *url, const uint8_t *buf, size_t *size)
{
    assert(s_curl_handle && "http_init wasn't called");
    assert(url && "invalid url");
    assert(buf && size);
    http_recv_buf tmp = { *size, 0, buf };
    LOG_TRACE("url %s", url);
    CURL *curl = curl_easy_duphandle(s_curl_handle);
    assert(curl);
    if (s_agentName)
        DO_CURL(setopt(curl, CURLOPT_USERAGENT, s_agentName));
    DO_CURL(setopt(curl, CURLOPT_FOLLOWLOCATION, 1));
    DO_CURL(setopt(curl, CURLOPT_URL, url));
    DO_CURL(setopt(curl, CURLOPT_WRITEDATA, &tmp));
    DO_CURL(setopt(curl, CURLOPT_WRITEFUNCTION, recv_data));
    DO_CURL(perform(curl));
    curl_easy_cleanup(curl);
    *size = tmp.offset;
}

void http_get_binary(const char *url, std::vector<uint8_t> &data)
{
    assert(s_curl_handle && "http_init wasn't called");
    assert(url && "invalid url");
    LOG_TRACE("url %s", url);
    CURL *curl = curl_easy_duphandle(s_curl_handle);
    assert(curl);
    if (s_agentName)
        DO_CURL(setopt(curl, CURLOPT_USERAGENT, s_agentName));
    DO_CURL(setopt(curl, CURLOPT_FOLLOWLOCATION, 1));
    DO_CURL(setopt(curl, CURLOPT_URL, url));
    DO_CURL(setopt(curl, CURLOPT_WRITEDATA, &data));
    DO_CURL(setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_vector));
    DO_CURL(perform(curl));
    curl_easy_cleanup(curl);
}

void http_get_string(const char *url, std::string &data)
{
    assert(s_curl_handle && "http_init wasn't called");
    assert(url && "invalid url");
    LOG_TRACE("url %s", url);
    CURL *curl = curl_easy_duphandle(s_curl_handle);
    assert(curl);
    if (s_agentName)
        DO_CURL(setopt(curl, CURLOPT_USERAGENT, s_agentName));
    DO_CURL(setopt(curl, CURLOPT_FOLLOWLOCATION, 1));
    DO_CURL(setopt(curl, CURLOPT_URL, url));
    DO_CURL(setopt(curl, CURLOPT_WRITEDATA, &data));
    DO_CURL(setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_string));
    DO_CURL(perform(curl));
    curl_easy_cleanup(curl);
}

bool http_get_file(const char *url, const char *filename)
{
    assert(s_curl_handle && "http_init wasn't called");
    assert(url && "invalid url");
    assert(filename && "invalid filename");
    FILE *fp = fopen(filename, "wb");
    if (!fp)
        return false;
    LOG_TRACE("url %s", url);
    CURL *curl = curl_easy_duphandle(s_curl_handle);
    assert(curl);
    if (s_agentName)
        DO_CURL(setopt(curl, CURLOPT_USERAGENT, s_agentName));
    DO_CURL(setopt(curl, CURLOPT_FOLLOWLOCATION, 1));
    DO_CURL(setopt(curl, CURLOPT_URL, url));
    DO_CURL(setopt(curl, CURLOPT_WRITEDATA, fp));
    DO_CURL(perform(curl));
    curl_easy_cleanup(curl);
    fclose(fp);
    return true;
}