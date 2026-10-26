// Test-only implementation of the Redis commands used by the feedback endpoint.
export function redisFixture() {
  const hashes = new Map(), sorted = new Map(), strings = new Map();
  return commands => commands.map(([command, key, ...args]) => {
    let result;
    if (command === 'HSET') { if (!hashes.has(key)) hashes.set(key, new Map()); hashes.get(key).set(args[0], args[1]); result = 1; }
    else if (command === 'HGET') result = hashes.get(key)?.get(args[0]) ?? null;
    else if (command === 'HMGET') result = args.map(field => hashes.get(key)?.get(field) ?? null);
    else if (command === 'HDEL') result = Number(hashes.get(key)?.delete(args[0]) || false);
    else if (command === 'ZADD') { if (!sorted.has(key)) sorted.set(key, new Map()); sorted.get(key).set(args[1], Number(args[0])); result = 1; }
    else if (command === 'ZREM') result = Number(sorted.get(key)?.delete(args[0]) || false);
    else if (command === 'ZRANGE') {
      const entries = [...(sorted.get(key) || [])].sort((a, b) => b[1] - a[1]);
      result = entries.slice(Number(args[0]), Number(args[1]) + 1).map(([id]) => id);
    } else if (command === 'SET') {
      if ((strings.get(key) || 0) > Date.now()) result = null;
      else { strings.set(key, Date.now() + Number(args[3]) * 1000); result = 'OK'; }
    } else throw new Error(`Unsupported fixture command: ${command}`);
    return { result };
  });
}
