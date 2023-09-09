// Related documentation: https://everything.curl.dev/libcurl/examples/get

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <curl/curl.h>

std::string folder_name(const char *file, unsigned long int file_len);

class camic_notifier {
public:
    // Call initialize once per program.
    // This takes place of the constructor.
    // It would be better to define it as and use it as a constructor,
    // but libcurl docs says on Windows, libcurl can't be initialized
    // from static constructor if being defined in a DLL because
    // libcurl cannot load other libraries while Windows would be holding the loader lock.
    // This applies because this plugin becomes a DLL
    static void initialize();

    static std::string escape(std::string s);
    static void notify(std::string url);

    ~camic_notifier();
private:
    static bool ready;
    static std::string origin; // https://caracal etc.
};
