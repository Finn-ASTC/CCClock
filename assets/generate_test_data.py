"""Generate a ~19 MB JSON test file for JSON parser benchmarking.
   Target: nested objects, mixed types, arrays — realistic workload."""

import json
import random
import string
import sys
import os

TARGET_MB = 19
OUTPUT_PATH = os.path.join(os.path.dirname(__file__), "bench_19mb.json")

random.seed(42)

LANGUAGES = [
    "Python", "C", "C++", "Rust", "Go", "JavaScript", "TypeScript",
    "Java", "Kotlin", "Swift", "Ruby", "PHP", "Haskell", "Scala",
    "Elixir", "Clojure", "Lua", "Perl", "R", "Zig", "Nim", "Dart",
]

STATUSES = ["active", "inactive", "pending", "blocked", "completed"]


def random_string(min_len=4, max_len=48):
    length = random.randint(min_len, max_len)
    return ''.join(random.choices(string.ascii_letters + string.digits, k=length))


def random_scores(count=12):
    return [round(random.uniform(0.0, 100.0), 2) for _ in range(count)]


def make_user(i):
    return {
        "id": i,
        "name": f"user_{random_string(6, 16)}",
        "email": f"{random_string(3, 10)}@{random_string(3, 8)}.com",
        "age": random.randint(18, 80),
        "score": round(random.uniform(0.0, 100.0), 2),
        "active": random.choice([True, False]),
        "language": random.choice(LANGUAGES),
        "status": random.choice(STATUSES),
        "tags": [random.choice(["backend", "frontend", "devops", "data", "mobile", "embedded"])
                 for _ in range(random.randint(1, 5))],
        "scores": random_scores(random.randint(4, 20)),
        "metadata": {
            "created_at": f"202{random.randint(0,4)}-{random.randint(1,12):02d}-{random.randint(1,28):02d}",
            "updated_at": f"202{random.randint(0,4)}-{random.randint(1,12):02d}-{random.randint(1,28):02d}",
            "login_count": random.randint(0, 5000),
            "ratio": round(random.uniform(0.0, 1.0), 6),
            "nested": {
                "level2_a": random.randint(0, 1000),
                "level2_b": round(random.uniform(0.0, 1000.0), 4),
                "flag": random.choice([True, False, None]),
            },
        },
        "friends": [random.randint(0, 20000) for _ in range(random.randint(0, 10))],
    }


def main():
    data = {"users": []}
    users = data["users"]

    sample = json.dumps(make_user(0), separators=(",", ":"))
    bytes_per_user = len(sample.encode("utf-8"))
    target_bytes = TARGET_MB * 1024 * 1024
    estimated_count = target_bytes // bytes_per_user

    print(f"Bytes per sample user: {bytes_per_user}")
    print(f"Estimated user count:   {estimated_count}")

    # Generate in chunks with progress
    chunk = 5000
    total = 0
    total_bytes = 0
    for base in range(0, estimated_count, chunk):
        batch = min(chunk, estimated_count - base)
        for i in range(base, base + batch):
            users.append(make_user(i))
        total += batch
        current = json.dumps(data, separators=(",", ":")).encode("utf-8")
        total_bytes = len(current)
        pct = total_bytes / target_bytes * 100
        print(f"  {total} users  →  {total_bytes / (1024*1024):.1f} MB  ({pct:.0f}%)")
        if total_bytes >= target_bytes:
            break

    full_json = json.dumps(data, separators=(",", ":"))
    final_size = len(full_json.encode("utf-8"))
    print(f"\nFinal: {len(data['users'])} users, {final_size / (1024*1024):.2f} MB")

    with open(OUTPUT_PATH, "w", encoding="utf-8") as f:
        f.write(full_json)

    # Verify roundtrip
    re_parsed = json.loads(full_json)
    assert len(re_parsed["users"]) == len(data["users"]), "Roundtrip mismatch!"
    print(f"Roundtrip verified OK → {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
