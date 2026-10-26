const releases = 'https://github.com/Bubliska/movie-master/releases/latest';
const patterns = { mac: /AppleSilicon\.dmg$/i, windows: /DOS-Windows\.zip$/i, checksums: /^SHA256\.txt$/i };

export default {
  async fetch(request) {
    const pattern = patterns[new URL(request.url).searchParams.get('platform')];
    if (!pattern) return new Response('Choose a download.', { status: 400 });
    let destination = releases;
    try {
      const result = await fetch('https://api.github.com/repos/Bubliska/movie-master/releases/latest', {
        headers: { Accept: 'application/vnd.github+json', 'User-Agent': 'Movie-Master-Website' },
        signal: AbortSignal.timeout(6000)
      });
      if (result.ok) {
        const release = await result.json();
        const asset = release.assets?.find(item => pattern.test(item.name));
        if (asset?.browser_download_url?.startsWith('https://github.com/Bubliska/movie-master/releases/download/')) destination = asset.browser_download_url;
      }
    } catch { /* The latest-release page remains available if the API cannot be reached. */ }
    return new Response(null, { status: 307, headers: { Location: destination, 'Cache-Control': 'public, s-maxage=300, stale-while-revalidate=3600' } });
  }
};
