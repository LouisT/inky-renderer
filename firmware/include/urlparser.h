#pragma once

#include <Arduino.h>
#include <map>

namespace URLParser
{
    // Structure to hold a username and password for basic authentication
    struct BasicAuth
    {
        String username;
        String password;

        // Check if basic auth is set
        bool exists() const;

        // Encode the basic auth as a base64 string
        String encode() const;
    };

    // Class to parse a URL and provide convenience methods to manipulate its components
    class Parser
    {
    public:
        // Construct a parser from a URL string
        Parser(const String &url);

        // Reconstruct the full URL
        String getURL(const bool mask = false) const;

        // Set a parameter in the query string
        bool setParam(const String &key, const String &value);

        // Get the value of a parameter in the query string
        String getParam(const String &key) const;

        // Check if a parameter exists in the query string
        bool hasParam(const String &key) const;

        // Remove a parameter from the query string
        bool removeParam(const String &key);

        // Set the basic auth
        void setBasicAuth(const String &username, const String &password);

        // Clear the basic auth
        void clearBasicAuth();

        // Check if basic auth is set
        bool hasBasicAuth() const;

        // Get the basic auth
        BasicAuth getBasicAuth() const;

        // Get the path
        String getPath() const;

        // Set the path
        void setPath(const String &newPath);

        // Expand the path/query string recursively
        template <typename... Args>
        void expandPath(const String &segment, const Args &...rest)
        {
            expandPathSingle(segment);
            expandPath(rest...); // Recursively handle remaining arguments
        }

        // Overload of expandPath with no arguments
        void expandPath() {}

    private:
        // Handle a single path/query string
        void expandPathSingle(const String &segment);

        // Parse the URL
        void parseUrl(const String &url);

        // Parse the query string into a map
        std::map<String, String> parseParams(const String &query);

        String protocol;                 // e.g. "http"
        String domain;                   // e.g. "example.com"
        String path;                     // e.g. "/path/to/resource"
        BasicAuth basicAuth;             // e.g. "username:password"
        std::map<String, String> params; // e.g. {"param1": "value1", "param2": "value2"}
    };

    // URL-encode a string
    String urlEncode(const String &value);

    // URL-decode a string
    String urlDecode(const String &value);
}
