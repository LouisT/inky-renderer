// Gets screenshots from RAWG.io based on various filters or specific game identifiers.
export default async function RAWG({
    apiKey,
    filters = {},
    count = 6,
    minWidth = 0,
    minHeight = 0,
    safePageCap = 200,
    numGames = 1,
    gameId = null,
    gameSlug = null,
    gameSearch = null,
}) {
    const base = "https://api.rawg.io/api",
        pageSize = 40,
        maxPages = 500,
        sourceNote = "Images & data (c) RAWG.io",
        need = Math.max(1, Number(numGames) || 1);

    const buildParamsFromFilters = (f = {}, size = pageSize) => {
        const p = new URLSearchParams({ key: apiKey, page_size: String(size) }),
            map = ['platforms', 'genres', 'tags', 'developers', 'publishers', 'dates', 'search', 'ordering'];
        for (const k of map) {
            if (f[k] !== undefined && f[k] !== null && String(f[k]).length)
                p.set(k, Array.isArray(f[k]) ? f[k].join(",") : String(f[k]));
        }
        return p;
    };

    const httpGetWithRetry = async (url) => {
        for (let attempt = 1; attempt <= 3; attempt++) {
            const res = await fetch(url);
            if (res.ok)
                return res.json();
            if ((res.status === 429 || res.status >= 500) && attempt < 3) {
                await new Promise(r => setTimeout(r, 400 * attempt + Math.random() * 300));
                continue;
            }
            const body = await res.text().catch(() => ""),
                err = new Error(`HTTP ${res.status}: ${body || res.statusText}`);
            err.status = res.status;
            err.body = body;
            throw err;
        }
    };

    const pickAndFormat = (shots) => {
        const qualified = shots.filter(s => {
            const w = Number(s.width || 0),
                h = Number(s.height || 0);
            return (!minWidth || w >= minWidth) && (!minHeight || h >= minHeight) && s.image;
        });
        const pool = (qualified.length ? qualified : shots).slice();
        for (let i = pool.length - 1; i > 0; i--) {
            const j = Math.floor(Math.random() * (i + 1));
            [pool[i], pool[j]] = [pool[j], pool[i]];
        }
        return pool.slice(0, Math.min(count, pool.length)).map(s => ({ id: s.id, url: s.image, width: s.width || null, height: s.height || null }));
    };

    const fromData = (data) => ({
        id: data.id,
        slug: data.slug,
        name: data.name,
        released: data.released,
        background: data.background_image
    });

    let explicitGame = null;
    if (gameId) {
        explicitGame = fromData(await httpGetWithRetry(`${base}/games/${gameId}?key=${apiKey}`));
    } else if (gameSlug) {
        explicitGame = fromData(await httpGetWithRetry(`${base}/games/${encodeURIComponent(gameSlug)}?key=${apiKey}`));
    } else if (gameSearch) {
        explicitGame = fromData((await httpGetWithRetry(`${base}/games?${(new URLSearchParams({
            key: apiKey,
            search: gameSearch,
            page_size: "10"
        })).toString()}`))?.results?.[0]);
    }

    if (explicitGame) {
        const data = await httpGetWithRetry(`${base}/games/${explicitGame.id}/screenshots?key=${apiKey}&page_size=${pageSize}`),
            shots = Array.isArray(data.results) ? data.results : [];
        if (!shots.length)
            throw new Error(`No screenshots found for "${explicitGame.name}".`);
        const picked = pickAndFormat(shots);
        return {
            game: explicitGame,
            screenshots: picked,
            artworks: picked.map(p => p.url),
            source: sourceNote
        };
    }

    const results = [],
        seen = new Set();
    let guard = 0;

    while (results.length < need && guard < need * 10) {
        guard++;
        let totalPages = 1;
        {
            const params = buildParamsFromFilters(filters, 1),
                probe = await httpGetWithRetry(`${base}/games?${params.toString()}`),
                total = Math.max(Number(probe.count || 0), 0),
                calc = Math.ceil(total / pageSize);
            totalPages = Math.max(1, Math.min(calc || 1, maxPages, Math.max(1, safePageCap)));
        }
        let items = [];
        {
            let hi = totalPages,
                attempts = 0;
            while (attempts < 8) {
                attempts++;
                const page = 1 + Math.floor(Math.random() * hi),
                    params = buildParamsFromFilters(filters, pageSize);
                params.set("page", String(page));
                try {
                    const data = await httpGetWithRetry(`${base}/games?${params.toString()}`);
                    if (!Array.isArray(data.results) || data.results.length === 0) {
                        hi = Math.max(1, page - 1);
                        continue;
                    }
                    items = data.results;
                    break;
                } catch (err) {
                    const invalid = err && (err.status === 404 || (typeof err.body === "string" && /invalid page/i.test(err.body)));
                    if (invalid) {
                        hi = Math.max(1, Math.min(hi - 1, Math.max(1, Math.floor(hi * 0.7))));
                        continue;
                    }
                    throw err;
                }
            }
            if (!items.length) {
                const first = buildParamsFromFilters(filters, pageSize);
                first.set("page", "1");
                const fall = await httpGetWithRetry(`${base}/games?${first.toString()}`);
                items = Array.isArray(fall.results) ? fall.results : [];
            }
        }

        if (!items.length)
            continue;
        for (let i = items.length - 1; i > 0; i--) {
            const j = Math.floor(Math.random() * (i + 1));
            [items[i], items[j]] = [items[j], items[i]];
        }

        const g = items[0] || null;
        if (!g || seen.has(g.slug))
            continue;

        const screenData = await httpGetWithRetry(`${base}/games/${g.id}/screenshots?key=${apiKey}&page_size=${pageSize}`),
            shots = Array.isArray(screenData.results) ? screenData.results : [],
            picked = pickAndFormat(shots);
        if (!picked.length)
            continue;

        seen.add(g.slug);
        results.push({
            game: fromData(g),
            screenshots: picked,
            artworks: picked.map(p => p.url),
            source: sourceNote,
        });
    }

    if (!results.length)
        throw new Error("Could not find random games with screenshots for the given filters.");

    return need === 1 ? results[0] : results;
}
