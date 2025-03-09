#include "urlparser.h"
#include <base64.h>

namespace URLParser
{
    bool BasicAuth::exists() const
    {
        return !username.isEmpty() || !password.isEmpty();
    }

    String BasicAuth::encode() const
    {
        return base64::encode(username + ":" + password);
    }

    Parser::Parser(const String &url)
    {
        parseUrl(url);
    }

    String Parser::getURL() const
    {
        String url = protocol + "://";

        if (basicAuth.exists())
        {
            url += basicAuth.username + ":" + basicAuth.password + "@";
        }

        url += domain;

        if (!path.isEmpty())
        {
            url += path;
        }

        if (!params.empty())
        {
            url += "?";
            bool first = true;
            for (const auto &param : params)
            {
                if (!first)
                    url += "&";
                first = false;
                url += param.first + "=" + param.second;
            }
        }
        return url;
    }

    bool Parser::setParam(const String &key, const String &value)
    {
        params[key] = value;
        return true;
    }

    String Parser::getParam(const String &key) const
    {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : "";
    }

    bool Parser::hasParam(const String &key) const
    {
        return params.find(key) != params.end();
    }

    bool Parser::removeParam(const String &key)
    {
        return params.erase(key) > 0;
    }

    void Parser::setBasicAuth(const String &username, const String &password)
    {
        basicAuth.username = username;
        basicAuth.password = password;
    }

    void Parser::clearBasicAuth()
    {
        basicAuth.username.clear();
        basicAuth.password.clear();
    }

    bool Parser::hasBasicAuth() const
    {
        return basicAuth.exists();
    }

    BasicAuth Parser::getBasicAuth() const
    {
        return basicAuth;
    }

    void Parser::setPath(const String &newPath)
    {
        path = newPath.startsWith("/") ? newPath : ("/" + newPath);
    }

    String Parser::getPath() const
    {
        return path;
    }

    void Parser::extendPath(const String &additionalPath)
    {
        // Do nothing if the additional path is empty.
        if (additionalPath.length() == 0)
            return;

        if (path.length() == 0)
        {
            // If current path is empty, set it with a leading slash.
            path = additionalPath.startsWith("/") ? additionalPath : ("/" + additionalPath);
        }
        else
        {
            // Remove trailing slash from the current path if it's not just "/".
            if (path.length() > 1 && path.endsWith("/"))
            {
                path = path.substring(0, path.length() - 1);
            }
            // Remove leading slash from additionalPath if present.
            String temp = additionalPath.startsWith("/") ? additionalPath.substring(1) : additionalPath;
            path += "/" + temp;
        }
    }

    std::map<String, String> Parser::parseParams(const String &query)
    {
        std::map<String, String> parsedParams;
        int start = 0;
        while (start < query.length())
        {
            int equalPos = query.indexOf('=', start);
            if (equalPos == -1)
                break;
            int ampPos = query.indexOf('&', equalPos);
            String key = query.substring(start, equalPos);
            String value = (ampPos == -1) ? query.substring(equalPos + 1) : query.substring(equalPos + 1, ampPos);
            parsedParams[key] = value;
            start = (ampPos == -1) ? query.length() : ampPos + 1;
        }
        return parsedParams;
    }

    void Parser::parseUrl(const String &url)
    {
        basicAuth.username.clear();
        basicAuth.password.clear();
        params.clear();

        int protocolEnd = url.indexOf("://");
        protocol = (protocolEnd != -1) ? url.substring(0, protocolEnd) : "http";
        int domainStart = (protocolEnd != -1) ? protocolEnd + 3 : 0;

        int authEnd = url.indexOf('@', domainStart);
        int pathStart = url.indexOf('/', domainStart);
        int queryStart = url.indexOf('?', domainStart);

        if (authEnd != -1 && (pathStart == -1 || authEnd < pathStart))
        {
            String authString = url.substring(domainStart, authEnd);
            int colonPos = authString.indexOf(':');
            if (colonPos != -1)
            {
                basicAuth.username = authString.substring(0, colonPos);
                basicAuth.password = authString.substring(colonPos + 1);
            }
            domainStart = authEnd + 1;
        }

        if (pathStart != -1)
        {
            domain = url.substring(domainStart, pathStart);
            path = (queryStart != -1) ? url.substring(pathStart, queryStart) : url.substring(pathStart);
        }
        else
        {
            domain = (queryStart != -1) ? url.substring(domainStart, queryStart) : url.substring(domainStart);
            path.clear();
        }

        if (queryStart != -1)
        {
            String queryStr = url.substring(queryStart + 1);
            params = parseParams(queryStr);
        }
    }
}
