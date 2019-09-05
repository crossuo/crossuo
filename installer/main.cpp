#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <string>

#include <tinyxml2.h>
#include <curl/curl.h>

#define DO_CURL(x)                                                                                 \
    do                                                                                             \
    {                                                                                              \
        CURLcode r = curl_easy_##x;                                                                \
        if (r != CURLE_OK)                                                                         \
            fprintf(stdout, __FILE__ ":%d:ERROR:%d: %s\n", __LINE__, r, curl_easy_strerror(r));    \
    } while (0)

namespace loguru
{
const char *log_system_name(int)
{
    return "none";
}
}; // namespace loguru

size_t recv_data_string(const char *data, size_t size, size_t nmemb, std::string *str)
{
    assert(data && str && size && nmemb);
    const int amount = size * nmemb;
    str->append(data, amount);
    return amount;
}

size_t recv_data_vector(const char *data, size_t size, size_t nmemb, std::vector<char> *vec)
{
    assert(data && vec && size && nmemb);
    const int amount = size * nmemb;
    vec->insert(vec->end(), data, data + amount);
    return amount;
}

size_t debug_data(const char *data, size_t size, size_t nmemb, void *)
{
    assert(data && data && size && nmemb);
    fprintf(stdout, "%zu %zu\n", size, nmemb);
    fprintf(stdout, "%s\n\n", data);
    return size * nmemb;
}

int main(int argc, char **argv)
{
    curl_global_init(CURL_GLOBAL_ALL);
    curl_version_info_data *info = curl_version_info(CURLVERSION_NOW);
    fprintf(
        stdout,
        "libcurl %s (%s, %s, %s, libz/%s, tinyxml2/%d.%d.%d)\n",
        info->version,
        info->host,
        info->ssl_version,
        info->libssh_version,
        info->libz_version,
        TINYXML2_MAJOR_VERSION,
        TINYXML2_MINOR_VERSION,
        TINYXML2_PATCH_VERSION);
    CURL *curl = curl_easy_init();

    std::string str;
    std::vector<char> buffer;
    DO_CURL(setopt(curl, CURLOPT_URL, "http://crossuo.com/"));
    //DO_CURL(setopt(curl, CURLOPT_WRITEDATA, &buffer));
    //DO_CURL(setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_vector));
    //DO_CURL(setopt(curl, CURLOPT_WRITEFUNCTION, debug_data));
    DO_CURL(setopt(curl, CURLOPT_WRITEDATA, &str));
    DO_CURL(setopt(curl, CURLOPT_WRITEFUNCTION, recv_data_string));
    DO_CURL(perform(curl));

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    tinyxml2::XMLPrinter printer;
    printer.OpenElement("foo");
    printer.PushAttribute("foo", "bar");
    printer.CloseElement();
    fprintf(stdout, "tinyxml:%s", printer.CStr());
    //fprintf(stdout, "bytes received: %zu\n", buffer.size());
    fprintf(stdout, "bytes received: %zu\n", str.size());
    fprintf(stdout, "%s", str.c_str());

    return 0;
}
