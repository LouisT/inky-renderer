import imageProviders from './images.mjs';
import renderProviders from './renders.mjs';
import remoteRenders from './remotes.mjs';

// Combine into a single object
const providers = {
    ...applyTypes(imageProviders), // Apply image type to each provider
    ...applyTypes(renderProviders, "render"), // Apply render type to each provider
    ...applyTypes(remoteRenders, "remote"), // Apply remote type to each provider
};

// Create an array of provider definitions with no aliases
const definitions = Object.entries(providers).map(([name, value]) => ({ name, ...value })).filter(p => !p.alias)

// Apply types to each provider object within the providers object
function applyTypes(providers, _type = "image") {
    return Object.fromEntries(Object.entries(providers).map(([key, value]) => [key, { ...value, _type: value?._type ?? _type }]))
}

export {
    providers as default,
    providers,
    definitions
}