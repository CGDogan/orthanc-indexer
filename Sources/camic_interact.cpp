#include "camic_interact.h"
#include "camic_md5.h"
#include <stdlib.h>
#include "../Resources/Orthanc/Plugins/OrthancPluginCppWrapper.h"
#include <SerializationToolbox.h>

camic_notifier camicroscope;
std::string camic_notifier::origin;

// Change to 1 for debugging
#ifndef CURL_VERBOSE
#define CURL_VERBOSE 0
#endif

// For connection verification: Counts the number of bytes read.
// Use with
// int a = 0;
// curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&a);
// curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, byte_counter);
/* static size_t byte_counter(char *contents, size_t size, size_t nmemb, unsigned int *buffer)
{
    (void)contents;
    size_t realsize = size * nmemb;
    *buffer += realsize;
    return realsize;
}*/

#if defined(CURL_VERBOSE) && CURL_VERBOSE == 1
static size_t response_handler(char *contents, size_t size, size_t nmemb, void *buffer)
{
    char x[size*nmemb+1];
    x[size*nmemb] = 0;
    for (int i = 0; i < size*nmemb; i++)
    {
        x[i] = contents[i];
    }
    fprintf(stderr, "notifier result: %s\n", x);
    return size * nmemb;
}
#else
// If we aren't going to use the response
// curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, NULL);
// curl_easy_setopt(easyhandle, CURLOPT_WRITEFUNCTION, response_handler);
static size_t response_handler(char *contents, size_t size, size_t nmemb, void *buffer)
{
    (void)contents;
    (void)buffer;
    return size * nmemb;
}
#endif

bool camic_notifier::ready = false;

void camic_notifier::initialize()
{
    curl_global_init(CURL_GLOBAL_ALL);
    curl_version_info_data *data = curl_version_info(CURLVERSION_NOW);
    if (data->age < 0x075500) {
        fprintf(stderr, "Curl minimum 7.85 recommended\n");
    }

    // Try HTTPS, then HTTP
    const char *caracal_host = getenv("CARACAL_BACK_HOST_PORT");
    if (!caracal_host || caracal_host[0] == 0)
    {
        fprintf(stderr, "ENV var CARACAL_BACK_HOST_PORT is not set, Dicom server won't work well. Set CARACAL_BACK_HOST_PORT=ca-back:1441\n");
        fprintf(stderr, "caMicroscope dicomsrv could not connect to caMicroscope caracal.\n"
            "if dicomsrv is being run without caMicroscope, no action is necessary.\n");
        return;
    }
    std::string caracal(caracal_host);
    std::string url;

    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_VERBOSE, CURL_VERBOSE); // CURLOPT_HEADER can also be considered
    // Note: to print to stdout (curl default), you may remove lines with CURLOPT_WRITEDATA and CURLOPT_WRITEFUNCTION
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_handler);
    url = "https://" + caracal + "/loader/test";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK)
    {
        origin = "https://" + caracal;
        curl_easy_cleanup(curl);
        ready = true;
        return;
    }
    const char *err_https = curl_easy_strerror(res);

    url = "http://" + caracal + "/loader/test";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    res = curl_easy_perform(curl);
    if (res == CURLE_OK)
    {
        origin = "http://" + caracal;
        curl_easy_cleanup(curl);
        ready = true;
        return;
    }
    const char *err_http = curl_easy_strerror(res);
    fprintf(stderr, "caMicroscope dicomsrv could not connect to caMicroscope caracal.\n"
                    "if dicomsrv is being run without caMicroscope, no action is necessary.\n"
                    "if not, HTTPS curl_easy_perform error: %s and for HTTP: %s\n",
            err_https, err_http);
    curl_easy_cleanup(curl);
}

std::string camic_notifier::escape(std::string s) {
    char *s2 = curl_easy_escape((CURL *)0, s.c_str(), s.size());
    std::string escaped(s2);
    curl_free(s2);
    return escaped;
}

void camic_notifier::notify(std::string url)
{
    if (!ready)
    {
        initialize();
    }
    if (!ready) {
        // No caMicroscope
        return;
    }
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1); // Thread safety
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_handler);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, CURL_VERBOSE);
    url = origin + url;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
#ifdef CURL_VERBOSE
    fprintf(stderr, "--URL GET: %s\n", url.c_str());
#endif
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK)
    {
        const char *err = curl_easy_strerror(res);
        fprintf(stderr, "caMicroscope dicomsrv failed to call %s. Error: %s\n", url.c_str(), err);
    }
    curl_easy_cleanup(curl);
}

camic_notifier::~camic_notifier() {
    curl_global_cleanup();
}

// caMicroscope uses series instance uid MD5 as common folder name
static std::string hash_id(std::string series_id) {
    static const char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    boost::uuids::detail::md5 hasher;
    hasher.process_bytes(&series_id[0], series_id.size());
    unsigned int hash_area[4];
    hasher.get_digest(hash_area);
    unsigned char *hash = reinterpret_cast<unsigned char *>(hash_area);
    std::string res;
    res.reserve(32);
    for (int i = 0; i < 16; i++) {
        res.push_back(hex[hash[i] >> 4]);
        res.push_back(hex[hash[i] & 15]);
    }
    return res;
}

// "" if not dicom
// md5 of series instance id otherwise
std::string folder_name(const char *file, unsigned long int file_len) {
    if (file_len < 135)
        return "";
    if (file[128] != 'D' || file[129] != 'I' || file[130] != 'C' || file[131] != 'M')
        return "";
    try
    {
        OrthancPlugins::OrthancString s;
        s.Assign(OrthancPluginDicomBufferToJson(OrthancPlugins::GetGlobalContext(), file, file_len,
                                                OrthancPluginDicomToJsonFormat_Short,
                                                OrthancPluginDicomToJsonFlags_None, 256));

        Json::Value json;
        s.ToJson(json);
        static const char *const SERIES_INSTANCE_UID = "0020,000e";
        return hash_id(Orthanc::SerializationToolbox::ReadString(json, SERIES_INSTANCE_UID)).substr(0, 10);
    }
    catch (const Orthanc::OrthancException &)
    {
        return "";
    }
}
