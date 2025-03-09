#ifndef URLPARSER_H
#define URLPARSER_H

#include <Arduino.h>
#include <map>

namespace URLParser
{
    struct BasicAuth
    {
        String username;
        String password;

        bool exists() const;
        String encode() const;
    };

    class Parser
    {
    public:
        Parser(const String &url);

        String getURL() const;

        bool setParam(const String &key, const String &value);
        String getParam(const String &key) const;
        bool hasParam(const String &key) const;
        bool removeParam(const String &key);

        void setBasicAuth(const String &username, const String &password);
        void clearBasicAuth();
        bool hasBasicAuth() const;
        BasicAuth getBasicAuth() const;

        void setPath(const String &newPath);
        String getPath() const;
        void extendPath(const String &additionalPath);

    private:
        void parseUrl(const String &url);
        std::map<String, String> parseParams(const String &query);
        String protocol;
        String domain;
        String path;
        BasicAuth basicAuth;
        std::map<String, String> params;
    };
}

#endif