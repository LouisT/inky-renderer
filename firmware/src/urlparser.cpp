#include "urlparser.h"
#include <base64.h>
#include <stdlib.h>

namespace URLParser
{
    // Checks if basic authentication credentials exist
    bool BasicAuth::exists() const
    {
        return !username.isEmpty() || !password.isEmpty();
    }

    // Encodes the basic authentication credentials in Base64 format
    String BasicAuth::encode() const
    {
        return base64::encode(username + ":" + password);
    }

    // Constructor that initializes the parser with a URL
    Parser::Parser(const String &url)
    {
        parseUrl(url);
    }

    // Returns the full URL, optionally masking authentication details
    String Parser::getURL(const bool mask) const
    {
        String url = protocol + "://";

        // Include authentication details if not masked
        if (!mask && basicAuth.exists())
        {
            url += basicAuth.username + ":" + basicAuth.password + "@";
        }

        url += domain;

        // Append the path if it exists
        if (!path.isEmpty())
        {
            url += path;
        }

        // Append query parameters if they exist
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

    // Sets a query parameter in the URL
    bool Parser::setParam(const String &key, const String &value)
    {
        params[key] = value;
        return true;
    }

    // Retrieves a query parameter by key
    String Parser::getParam(const String &key) const
    {
        auto it = params.find(key);
        return (it != params.end()) ? it->second : "";
    }

    // Checks if a query parameter exists
    bool Parser::hasParam(const String &key) const
    {
        return params.find(key) != params.end();
    }

    // Removes a query parameter by key
    bool Parser::removeParam(const String &key)
    {
        return params.erase(key) > 0;
    }

    // Sets basic authentication credentials
    void Parser::setBasicAuth(const String &username, const String &password)
    {
        basicAuth.username = username;
        basicAuth.password = password;
    }

    // Clears basic authentication credentials
    void Parser::clearBasicAuth()
    {
        basicAuth.username.clear();
        basicAuth.password.clear();
    }

    // Checks if basic authentication is set
    bool Parser::hasBasicAuth() const
    {
        return basicAuth.exists();
    }

    // Retrieves the basic authentication credentials
    BasicAuth Parser::getBasicAuth() const
    {
        return basicAuth;
    }

    // Sets the path for the URL
    void Parser::setPath(const String &newPath)
    {
        path = newPath.startsWith("/") ? newPath : ("/" + newPath);
    }

    // Retrieves the path from the URL
    String Parser::getPath() const
    {
        return path;
    }

    // Expands the URL path with a given segment
    void Parser::expandPathSingle(const String &segment)
    {
        if (segment.isEmpty())
            return;

        int questionPos = segment.indexOf('?');
        String pathPart = segment;
        String queryPart;

        // Separate path and query parameters if present
        if (questionPos != -1)
        {
            pathPart = segment.substring(0, questionPos);
            queryPart = segment.substring(questionPos + 1);
        }

        // Append to path if not empty
        if (!pathPart.isEmpty())
        {
            if (path.isEmpty())
            {
                path = pathPart.startsWith("/") ? pathPart : ("/" + pathPart);
            }
            else
            {
                if (path.length() > 1 && path.endsWith("/"))
                {
                    path.remove(path.length() - 1);
                }
                String trimmed = pathPart.startsWith("/") ? pathPart.substring(1) : pathPart;
                path += "/" + trimmed;
            }
        }

        // Parse and add query parameters if present
        if (!queryPart.isEmpty())
        {
            auto newParams = parseParams(queryPart);
            for (auto &kv : newParams)
            {
                setParam(kv.first, kv.second);
            }
        }
    }

    // Parses query parameters into a key-value map
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

    // Parses a URL into its components
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

        // Extract basic authentication credentials
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

        // Extract domain and path
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

        // Extract query parameters if present
        if (queryStart != -1)
        {
            String queryStr = url.substring(queryStart + 1);
            params = parseParams(queryStr);
        }
    }

    // URL-encode a string
    String urlEncode(const String &value)
    {
        String encoded;
        encoded.reserve(value.length());
        for (int i = 0; i < value.length(); i++)
        {
            char c = value.charAt(i);
            if ((c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~')
            {
                encoded += c;
            }
            else
            {
                char buf[4];
                sprintf(buf, "%%%02X", (unsigned char)c);
                encoded += buf;
            }
        }
        return encoded;
    }

    // URL-decode a string
    String urlDecode(const String &value)
    {
        String decoded;
        decoded.reserve(value.length());

        for (int i = 0; i < value.length(); i++)
        {
            char c = value.charAt(i);
            if (c == '%' && (i + 2) < value.length())
            {
                char h1 = value.charAt(i + 1);
                char h2 = value.charAt(i + 2);
                char hexStr[3] = {h1, h2, '\0'};
                int hexVal = (int)strtol(hexStr, NULL, 16);
                decoded += (char)hexVal;
                i += 2;
            }
            else
            {
                decoded += c;
            }
        }
        return decoded;
    }
}
