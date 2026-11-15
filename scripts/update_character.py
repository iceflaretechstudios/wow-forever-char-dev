import json
import pathlib
import urllib.request
from datetime import datetime, timezone

SUMMARY_URL = "https://iceflaredashboard.lanceshipley42.workers.dev/api/summary"
ARMORY_URL = "https://iceflaredashboard.lanceshipley42.workers.dev/api/armory/forever/iceflare-theundying"
OUTPUT = pathlib.Path("data/character.json")


def fetch_json(url):
    request = urllib.request.Request(
        url, headers={"User-Agent": "wow-forever-char-dev/1.1", "Accept": "application/json"}
    )
    with urllib.request.urlopen(request, timeout=25) as response:
        if response.status != 200:
            raise RuntimeError(f"Unexpected HTTP status {response.status}")
        return json.load(response)


def find_money(value, depth=0):
    """Find a clearly labeled coin balance; never confuse item sell prices with wallet gold."""
    if depth > 5 or not isinstance(value, dict):
        return None
    for key in ("money", "wallet", "currency", "balance", "goldBalance", "gold_balance"):
        nested = value.get(key)
        if isinstance(nested, dict):
            result = find_money(nested, depth + 1)
            if result:
                return result
    if all(k in value for k in ("gold", "silver", "copper")):
        try:
            coins = {k: int(value[k]) for k in ("gold", "silver", "copper")}
            if all(v >= 0 for v in coins.values()) and coins["silver"] < 100 and coins["copper"] < 100:
                return coins
        except (TypeError, ValueError):
            pass
    for key in ("character", "player", "armory", "data", "result", "icecloud", "iceCloud"):
        nested = value.get(key)
        if isinstance(nested, dict):
            result = find_money(nested, depth + 1)
            if result:
                return result
    return None


data = fetch_json(SUMMARY_URL)
character = data.get("character")
if not isinstance(character, dict) or character.get("name") != "Iceflare Theundying" or not isinstance(character.get("level"), int):
    raise ValueError("API response does not contain the expected Iceflare character data")

# The armory endpoint can include the IceCloud/D1 wallet, unlike /api/summary.
# Do not invent a balance or retain an old one as though it were fresh.
data.pop("money", None)
data.pop("moneySyncedAt", None)
try:
    armory = fetch_json(ARMORY_URL)
    money = find_money(armory)
    if money is None:
        print("Cloudflare armory responded, but no gold/silver/copper wallet was found")
    else:
        data["money"] = money
        data["moneySyncedAt"] = datetime.now(timezone.utc).isoformat()
        print(f"Gold synced: {money['gold']}g {money['silver']}s {money['copper']}c")
except (OSError, ValueError, json.JSONDecodeError) as exc:
    print(f"Gold sync unavailable; character snapshot will still update: {exc}")

data["snapshotFetchedAt"] = datetime.now(timezone.utc).isoformat()
OUTPUT.parent.mkdir(parents=True, exist_ok=True)
OUTPUT.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(f"Updated {character['name']} level {character['level']}")
