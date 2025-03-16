import imageProviders from './images.mjs';
import renderProviders from './renders.mjs';

// Combine into a single object
const providers = {
    ...applyTypes(imageProviders), // Apply image type to each provider
    ...applyTypes(renderProviders, "render"), // Apply render type to each provider
};

// Apply types to each provider object within the providers object
function applyTypes(providers, _type = "image") {
    return Object.fromEntries(Object.entries(providers).map(([key, value]) => [key, { ...value, _type }]))
}

export {
    providers as default,
    providers
}