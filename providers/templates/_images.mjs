// NY Times - https://developer.nytimes.com/files/poweredby_nytimes_200a.png
const nytimes = `data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAMgAAAAoAgMAAADGcl2PAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJbWFnZVJlYWR5ccllPAAAAAxQTFRFFhYW8fHxdnZ2uLi4DSzGmAAAApdJREFUOMvt1TFo20AUANBvuSqWjIqHKvaQoUMuDWk7HmQqgk5HGjCJv2VSl5wcOmiTm61goiS0W2iHKGQoNFA6JKWQJZ29aDAlkKFKM2bvbDoF90uWTeJm8dYhp+G4E8/3/f8/G3DsAbdkbFIBGpnxCR1mK/i6DlWOkklWZ+4LVzJPk/HadW8iAdpPTWm/rzBc7cx3Gp1mU6x21rrz27R2Tm4iHO0ifx6F9RAdIRyxtuQKx1nrChcdd5GNEn3fABVtk78LwmWBjW3LWFhaXhBO0zOEg9Kom6OE2RuWFhPdDBc3sHE8z8RSPSTSxPgUpxKMEvoMK4v2RHGWhY3P6KA4WXUqTDgukc6N5BMRQBvOzLehWIyJU3tS5YIylrEYSu1fUviKccrGqcujHzkANhZRAyLGWKRo5sZpmZjsmN8Bsnavt55sXa5EyWxf2l2aaHOl1/t9nfw0dwE06gIr2cqW+5WrZmqcJhlhDZStUUKBqUOipAQVLxiSkVPOzJxPTVY2rRa2aOvUbCXv2hZ20SaCMrjElysYP33yq5RrU12I+FWIEApF+PhQNVBu1pXanDyn/g+2LP/C9Hmb9UlgaW26MERgb4YjPLifm5o2VCxnvP09kBdRQgCmdUM30h57cwzZhHgfdhjmoDRx906eYc2QpyVdbmJCypNTLJ/vpqXEKmh9ckSLPBTgXk7hWFGt8hyXMCCPIQNpYFlKiZGS2S+o+YXZb9A2hyQaEP2gnUlv5WGcsH5gQYCaVeLHvhUQocC4PAqGgb36s3H17idkb4ah5k3q6ibltqLS1+fylKdkWs/rmau/MAnZJauVi8CfeUikptS4PGcpYT7z1SFRBqXtF3F9sOxea0gqZRST6iGNg9s/i/+L/AVC635diokBGQAAAABJRU5ErkJggg==`;

// Export all providers as a single object
const all = {
    nytimes,
};
export {
    all as default, all,

    // Export individual providers
    nytimes
};