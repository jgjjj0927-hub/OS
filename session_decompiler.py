# language: Python, file: session_decompiler.py, target: Windows/Linux, Python 3.10+
# разбирает .session (SQLite) Telethon: таблицы, auth_key, dc_id, update_state, entities
# умеет: info, dump -> json, rebuild из json в рабочий .session, probe через Telethon
import sqlite3
import base64
import json
import os
import sys
import argparse
import asyncio
from datetime import datetime

TELEGRAM_DC_IPS = {
    1: "149.154.175.53",
    2: "149.154.167.51",
    3: "149.154.175.100",
    4: "149.154.167.91",
    5: "91.108.56.130",
}


def _safe_b64(b):
    if b is None:
        return None
    return base64.b64encode(b).decode()


def _safe_unb64(s):
    if not s:
        return None
    return base64.b64decode(s)


def open_session(path: str) -> sqlite3.Connection:
    if not os.path.exists(path):
        raise FileNotFoundError(path)
    con = sqlite3.connect(path)
    con.row_factory = sqlite3.Row
    return con


def list_tables(con: sqlite3.Connection) -> list[str]:
    return [r[0] for r in con.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")]


def dump_table(con: sqlite3.Connection, table: str) -> list[dict]:
    rows = []
    try:
        for row in con.execute(f"SELECT * FROM {table}"):
            d = {}
            for k in row.keys():
                v = row[k]
                if isinstance(v, (bytes, bytearray)):
                    d[k] = {"__bytes_b64__": base64.b64encode(v).decode(), "len": len(v)}
                elif isinstance(v, datetime):
                    d[k] = {"__datetime__": v.isoformat()}
                else:
                    d[k] = v
            rows.append(d)
    except sqlite3.OperationalError as e:
        return [{"__error__": str(e)}]
    return rows


def dump_session(path: str) -> dict:
    con = open_session(path)
    out = {"file": os.path.basename(path), "size": os.path.getsize(path), "tables": {}}
    for t in list_tables(con):
        out["tables"][t] = dump_table(con, t)

    summary = {}
    if "sessions" in out["tables"]:
        for row in out["tables"]["sessions"]:
            if "__error__" in row:
                continue
            ak = row.get("auth_key")
            summary = {
                "dc_id": row.get("dc_id"),
                "server_address": row.get("server_address"),
                "port": row.get("port"),
                "auth_key_len": ak.get("len") if isinstance(ak, dict) else (len(ak) if ak else 0),
                "auth_key_b64": ak.get("__bytes_b64__") if isinstance(ak, dict) else None,
                "takeout_id": row.get("takeout_id"),
            }
            break
    out["summary"] = summary
    con.close()
    return out


def rebuild_session(dump: dict, out_path: str) -> str:
    if os.path.exists(out_path):
        os.remove(out_path)
    con = sqlite3.connect(out_path)
    con.execute("""CREATE TABLE sessions (
        dc_id INTEGER PRIMARY KEY, server_address TEXT, port INTEGER,
        auth_key BLOB, takeout_id INTEGER)""")
    con.execute("""CREATE TABLE entities (
        id INTEGER PRIMARY KEY, hash INTEGER NOT NULL, username TEXT,
        phone TEXT, name TEXT, date INTEGER)""")
    con.execute("""CREATE TABLE update_state (
        id INTEGER PRIMARY KEY, pts INTEGER, qts INTEGER, date INTEGER, seq INTEGER)""")

    s = dump.get("summary", {})
    ak = _safe_unb64(s.get("auth_key_b64"))
    if not ak:
        con.close()
        raise ValueError("нет auth_key в дампе — сессию не собрать")
    dc_id = s.get("dc_id") or 2
    con.execute(
        "INSERT INTO sessions (dc_id, server_address, port, auth_key, takeout_id) VALUES (?,?,?,?,?)",
        (dc_id, s.get("server_address") or TELEGRAM_DC_IPS.get(dc_id, "149.154.167.51"),
         s.get("port") or 443, ak, s.get("takeout_id"))
    )
    if "update_state" in dump.get("tables", {}):
        for row in dump["tables"]["update_state"]:
            if "__error__" in row:
                continue
            con.execute(
                "INSERT OR REPLACE INTO update_state (id, pts, qts, date, seq) VALUES (?,?,?,?,?)",
                (row.get("id", 1), row.get("pts"), row.get("qts"), row.get("date"), row.get("seq"))
            )
    if "entities" in dump.get("tables", {}):
        for row in dump["tables"]["entities"]:
            if "__error__" in row:
                continue
            con.execute(
                "INSERT OR REPLACE INTO entities (id, hash, username, phone, name, date) VALUES (?,?,?,?,?,?)",
                (row.get("id"), row.get("hash"), row.get("username"), row.get("phone"),
                 row.get("name"), row.get("date"))
            )
    con.commit()
    con.close()
    return out_path


async def probe_session(session_path: str, api_id: int, api_hash: str) -> dict:
    try:
        from telethon import TelegramClient
        from telethon.errors import (
            AuthKeyUnregisteredError, UserDeactivatedError,
            UserDeactivatedBanError, ApiIdInvalidError,
        )
    except ImportError:
        return {"ok": False, "error": "pip install telethon"}

    stem = session_path[:-8] if session_path.endswith(".session") else session_path
    client = TelegramClient(stem, api_id, api_hash, system_version="4.16.30-vxCUSTOM")
    try:
        await client.connect()
        if not await client.is_user_authorized():
            return {"ok": False, "error": "не авторизована (ключ не принят)"}
        me = await client.get_me()
        return {
            "ok": True, "id": me.id, "first_name": me.first_name,
            "last_name": me.last_name, "username": me.username,
            "phone": me.phone, "premium": getattr(me, "premium", False), "bot": me.bot,
        }
    except AuthKeyUnregisteredError:
        return {"ok": False, "error": "AuthKeyUnregistered — сессия отозвана"}
    except UserDeactivatedBanError:
        return {"ok": False, "error": "аккаунт забанен"}
    except UserDeactivatedError:
        return {"ok": False, "error": "аккаунт удалён"}
    except ApiIdInvalidError:
        return {"ok": False, "error": "api_id/api_hash не подходят"}
    except Exception as e:
        return {"ok": False, "error": f"{type(e).__name__}: {e}"}
    finally:
        try:
            await client.disconnect()
        except Exception:
            pass


def main():
    p = argparse.ArgumentParser(description="Telethon .session decompiler / rebuilder / prober")
    p.add_argument("action", choices=["dump", "rebuild", "probe", "info"])
    p.add_argument("path", help="путь к .session или к dump.json")
    p.add_argument("--out", help="куда писать (для dump/rebuild)")
    p.add_argument("--api-id", type=int, default=24565698)
    p.add_argument("--api-hash", default="5a084b434f8505ace703485b9da85040")
    args = p.parse_args()

    if args.action == "dump":
        d = dump_session(args.path)
        out = args.out or (args.path + ".dump.json")
        with open(out, "w", encoding="utf-8") as f:
            json.dump(d, f, indent=2, ensure_ascii=False)
        print(f"[dump] {out}")
        print(f"  size: {d['size']}  tables: {list(d['tables'].keys())}")
        s = d["summary"]
        print(f"  dc_id: {s.get('dc_id')}  server: {s.get('server_address')}:{s.get('port')}")
        print(f"  auth_key_len: {s.get('auth_key_len')}")
        if s.get("auth_key_b64"):
            print(f"  auth_key_b64: {s['auth_key_b64'][:64]}...")

    elif args.action == "rebuild":
        with open(args.path, encoding="utf-8") as f:
            d = json.load(f)
        out = args.out or "rebuilt.session"
        rebuild_session(d, out)
        print(f"[rebuild] {out}")

    elif args.action == "probe":
        res = asyncio.run(probe_session(args.path, args.api_id, args.api_hash))
        print(json.dumps(res, indent=2, ensure_ascii=False))

    elif args.action == "info":
        con = open_session(args.path)
        print(f"file: {args.path}")
        print(f"size: {os.path.getsize(args.path)}")
        for t in list_tables(con):
            n = con.execute(f"SELECT COUNT(*) FROM {t}").fetchone()[0]
            print(f"  table {t}: {n} rows")
        if "sessions" in list_tables(con):
            for row in con.execute("SELECT dc_id, server_address, port, length(auth_key) FROM sessions"):
                print(f"  session: dc_id={row[0]} server={row[1]}:{row[2]} auth_key_len={row[3]}")
        con.close()


if __name__ == "__main__":
    main()
