#!/usr/bin/env python3
"""Two-level factorial experiments over the SimdQuickHeap Opts flags.

Each flag is a factor at two levels, off and on. `design` prints the Opts values
of a design. `registry` prints the benchmark registry lines for them. `effects`
reads pass-*.csv files from one language and estimates each factor's effect:
within one pass and one (type, workload, size) cell, the mean log time of the
runs with the flag on minus the mean with it off. Passes are the replicates.

Bit 32 is read by no code. In a design it is a dummy factor whose effect is
pure noise, layout differences between instantiations included.
"""

import argparse
import csv
import html
import itertools
import json
import random
import math
from pathlib import Path
import re
import statistics

FACTORS = {"A1": 1, "A2": 2, "A3": 4, "A4": 8, "A5": 16, "A6": 64, "A7": 128, "A8": 256, "A9": 512, "dummy": 32}
WORKLOADS = ("MonotoneConstantSize", "RandomConstantSize", "WorstCaseDescending", "MostlyPushDecreasing", "HeapSort")
NAMED = {"quickheap": 0, "quickheap_aa": 32} | {
    f"quickheap_a{digits}": sum(FACTORS[f"A{d}"] for d in digits)
    for digits in ("1", "2", "3", "4", "5", "6", "7", "8", "9", "12", "123", "1234", "12345")}
# Two-sided Student t quantiles at 95% by degrees of freedom.
T95 = {1: 12.706, 2: 4.303, 3: 3.182, 4: 2.776, 5: 2.571, 6: 2.447, 7: 2.365, 8: 2.306, 9: 2.262,
       10: 2.228, 12: 2.179, 15: 2.131, 20: 2.086, 30: 2.042}


def t95(df):
    return T95[max(k for k in T95 if k <= df)]


def screening():
    """2^(10-5) resolution IV, 32 runs, with the negative generators so the all-off run is included."""
    base = ("A1", "A2", "A3", "A4", "A5")
    generators = {"A6": "A1 A2 A3 A4", "A7": "A1 A2 A3 A5", "A8": "A1 A2 A4 A5",
                  "A9": "A1 A3 A4 A5", "dummy": "A2 A3 A4 A5"}
    runs = []
    for levels in itertools.product((-1, 1), repeat=len(base)):
        run = dict(zip(base, levels))
        for factor, word in generators.items():
            run[factor] = -math.prod(run[f] for f in word.split())
        runs.append(run)
    return runs


def full(factors, fixed, half=False):
    """All level combinations of `factors`; with `half`, the last factor is the product of the others."""
    base = factors[:-1] if half else factors
    runs = []
    for levels in itertools.product((-1, 1), repeat=len(base)):
        run = {f: (1 if FACTORS[f] & fixed else -1) for f in FACTORS}
        run.update(zip(base, levels))
        if half:
            run[factors[-1]] = math.prod(levels)
        runs.append(run)
    return runs


def opts(run):
    return sum(FACTORS[f] for f, level in run.items() if level > 0)


def levels(value):
    return {f: (1 if value & bit else -1) for f, bit in FACTORS.items()}


def resolution(runs):
    """Shortest word in the defining relation: the smallest factor set whose product column is constant."""
    names = [f for f in FACTORS if len({run[f] for run in runs}) == 2]
    for size in range(1, len(names) + 1):
        for word in itertools.combinations(names, size):
            if len({math.prod(run[f] for f in word) for run in runs}) == 1:
                return size
    return math.inf


def heap_name(value):
    return next((name for name, bits in NAMED.items() if bits == value), f"quickheap_o{value}")


def heap_opts(raw):
    if raw.startswith("quickheap::SimdQuickHeap<"):
        args = [arg.strip() for arg in raw[raw.index("<") + 1:raw.rindex(">")].split(",")]
        return int(args[2]) if len(args) > 2 else 0
    if raw in NAMED:
        return NAMED[raw]
    match = re.fullmatch(r"quickheap_o(\d+)", raw)
    return int(match.group(1)) if match else None


def workload_name(raw):
    return re.sub(r"<.*>$", "", raw.rsplit("::", 1)[-1])


def load(directory):
    """Return {(pass, elem, workload, n): {opts: log of median ns per op}}."""
    cells = {}
    paths = sorted(Path(directory).glob("pass-*.csv"))
    if not paths:
        raise SystemExit(f"{directory}: no pass-*.csv files")
    for path in paths:
        samples = {}
        with path.open(newline="", encoding="utf-8") as stream:
            for row in csv.DictReader(stream):
                value = heap_opts(row["heap"])
                if value is None:
                    continue
                key = (path.stem, row["elem"], workload_name(row["workload"]), int(row["n"]))
                samples.setdefault(key, {}).setdefault(value, []).append(float(row["nanos"]) / int(row["ops"]))
        for key, by_opts in samples.items():
            cells[key] = {value: math.log(statistics.median(times)) for value, times in by_opts.items()}
    return cells


def interval(values):
    if not values:
        return math.nan, math.nan, math.nan
    mean = statistics.fmean(values)
    if len(values) < 2:
        return mean, math.nan, math.nan
    half = t95(len(values) - 1) * statistics.stdev(values) / math.sqrt(len(values))
    return mean, mean - half, mean + half


def percent(log_ratio):
    return 100 * math.expm1(log_ratio)


def contrasts(cells, design, terms):
    """Per pass and cell, each term's effect: mean log time at +1 minus mean at -1.

    A term is a tuple of factors; its level in a run is the product of theirs.
    Returns {term: {(pass, elem, workload, n): effect}}.
    """
    out = {term: {} for term in terms}
    for key, by_opts in cells.items():
        if not all(value in by_opts for value in design):
            continue
        for term in terms:
            high = [by_opts[value] for value in design if math.prod(levels(value)[f] for f in term) > 0]
            low = [by_opts[value] for value in design if math.prod(levels(value)[f] for f in term) < 0]
            out[term][key] = statistics.fmean(high) - statistics.fmean(low)
    return out


def summarize(per_cell, select):
    """Average a term's effect over the selected cells within each pass, then interval over passes."""
    by_pass = {}
    for (run, elem, workload, n), effect in per_cell.items():
        if select(elem, workload, n):
            by_pass.setdefault(run, []).append(effect)
    values = [statistics.fmean(v) for v in by_pass.values()]
    mean, low, high = interval(values)
    return {"change": percent(mean), "low": percent(low), "high": percent(high), "passes": len(values)}


def term_name(term):
    return "×".join(term)


def column(design, term):
    return tuple(math.prod(levels(value)[f] for f in term) for value in design)


def alias_chains(design, terms):
    """Group terms whose design columns are equal up to sign; return [(representative, [(sign, term)])]."""
    chains = {}
    for term in terms:
        col = column(design, term)
        sign = 1 if col[0] > 0 else -1
        chains.setdefault(tuple(sign * c for c in col), []).append((sign, term))
    return [(members[0][1], members) for members in chains.values()]


SHORT = {"MonotoneConstantSize": "Monotone", "RandomConstantSize": "Random", "WorstCaseDescending": "WorstDesc",
         "MostlyPushDecreasing": "PushDecr", "HeapSort": "HeapSort"}
SCOPES = {
    "workload": {"all": lambda e, w, n: True} | {SHORT[t]: (lambda e, w, n, t=t: w == t) for t in WORKLOADS},
    "n": {"all": lambda e, w, n: True} | {f"2^{k}": (lambda e, w, n, k=k: n == 2 ** k) for k in range(10, 23, 2)},
    "elem": {"all": lambda e, w, n: True} | {t: (lambda e, w, n, t=t: e == t) for t in ("i32", "i64")},
}


def effects_table(directory, design, terms, scopes):
    per_term = contrasts(load(directory), design, terms)
    return {term: {scope: summarize(per_term[term], select) for scope, select in scopes.items()} for term in terms}


def cell_text(cell):
    if math.isnan(cell["change"]):
        return f'{"-":^24}'
    mark = "*" if cell["low"] > 0 or cell["high"] < 0 else " "
    return f'{cell["change"]:+6.2f} [{cell["low"]:+6.2f},{cell["high"]:+6.2f}] {mark}'


def print_table(rows):
    """rows: [(label, {scope: cell})]. Starred cells have a 95% interval that excludes zero."""
    width = max(len(label) for label, _ in rows)
    print(" " * width + " | " + " | ".join(f"{h:^24}" for h in rows[0][1]))
    for label, by_scope in rows:
        print(f"{label:<{width}} | " + " | ".join(cell_text(cell) for cell in by_scope.values()))


def combinations(directory, baseline=0):
    cells = load(directory)
    table = {}
    for scope in ("all",) + WORKLOADS:
        by_opts = {}
        for (run, elem, workload, n), by in cells.items():
            if baseline not in by or (scope != "all" and workload != scope):
                continue
            for value, y in by.items():
                by_opts.setdefault(value, {}).setdefault(run, []).append(y - by[baseline])
        for value, runs in by_opts.items():
            mean, low, high = interval([statistics.fmean(v) for v in runs.values()])
            table.setdefault(value, {})[scope] = {"change": percent(mean), "low": percent(low),
                                                  "high": percent(high), "passes": len(runs)}
    return table


STEPS = (2, 5, 10, math.inf)
FASTER = ("#b7d3f6", "#6da7ec", "#2a78d6", "#184f95")
SLOWER = ("#f6cfce", "#ee8f8e", "#e34948", "#a32a29")
NEUTRAL = "#f0efec"


def fill(cell):
    if cell["low"] > 0 or cell["high"] < 0:
        ramp = FASTER if cell["change"] < 0 else SLOWER
        return ramp[next(i for i, bound in enumerate(STEPS) if abs(cell["change"]) < bound)], True
    return NEUTRAL, False


def label(change):
    digits = 1 if abs(change) < 9.95 else 0
    value = round(change, digits)
    return "0" if value == 0 else f"{value:+.{digits}f}".replace("-", "−")


def heatmap(caption, rows, groups):
    """rows: [(label, {scope: cell})]; groups: [(title, [scope])]. Draws one SVG grid."""
    ch, gap, top = 24, 12, 44
    cw = max(46, 8 + 7 * max(len(scope) for _, scopes in groups for scope in scopes))
    left = 12 + 7 * max(len(name) for name, _ in rows)
    width = left + sum(len(scopes) * cw + gap for _, scopes in groups)
    height = top + len(rows) * ch + 4
    out = [f'<svg viewBox="0 0 {width} {height}" width="{width}" height="{height}" role="img" '
           f'aria-label="{html.escape(caption)}">']
    for row, (name, _) in enumerate(rows):
        out.append(f'<text class="row" x="{left - 8}" y="{top + row * ch + ch / 2 + 4}" text-anchor="end">'
                   f'{html.escape(name)}</text>')
    x0 = left
    for title, scopes in groups:
        out.append(f'<text class="group" x="{x0}" y="14">{html.escape(title)}</text>')
        if x0 > left:
            out.append(f'<line x1="{x0 - gap / 2}" x2="{x0 - gap / 2}" y1="{top}" y2="{height}" class="sep"/>')
        for col, scope in enumerate(scopes):
            x = x0 + col * cw
            out.append(f'<text class="size" x="{x + cw / 2}" y="{top - 8}" text-anchor="middle">{html.escape(scope)}</text>')
            for row, (name, cells) in enumerate(rows):
                cell = cells.get(scope)
                if cell is None or math.isnan(cell["change"]):
                    continue
                y = top + row * ch
                color, sure = fill(cell)
                ink = "#ffffff" if sure and abs(cell["change"]) >= STEPS[1] else "#0b0b0b"
                tip = (f'{caption}, {name}, {scope}: {cell["change"]:+.2f}% '
                       f'[{cell["low"]:+.2f}, {cell["high"]:+.2f}] over {cell["passes"]} passes')
                out.append(f'<g><title>{html.escape(tip)}</title>'
                           f'<rect x="{x + 1}" y="{y + 1}" width="{cw - 2}" height="{ch - 2}" rx="3" fill="{color}"/>'
                           f'<text x="{x + cw / 2}" y="{y + ch / 2 + 4}" text-anchor="middle" fill="{ink}" '
                           f'font-weight="{600 if sure else 400}" opacity="{1 if sure else 0.55}">'
                           f'{label(cell["change"])}</text></g>')
        x0 += len(scopes) * cw + gap
    out.append("</svg>")
    return "\n".join(out)


def legend():
    names = ["< 2%", "2–5%", "5–10%", "≥ 10%"]
    items = [f'<span><i style="background:{c}"></i>faster {t}</span>' for t, c in zip(names, FASTER)]
    items.append(f'<span><i style="background:{NEUTRAL}"></i>95% interval includes 0</span>')
    items += [f'<span><i style="background:{c}"></i>slower {t}</span>' for t, c in zip(names, SLOWER)]
    return '<p class="legend">' + "".join(items) + "</p>"


PAGE = """<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<title>{title}</title>
<style>
body {{ margin: 24px; background: #f9f9f7; color: #0b0b0b; font: 14px system-ui, -apple-system, "Segoe UI", sans-serif; }}
section {{ background: #fcfcfb; border: 1px solid rgba(11,11,11,0.10); border-radius: 8px; padding: 16px 20px; margin-bottom: 20px; overflow-x: auto; }}
h1 {{ font-size: 20px; }} h2 {{ font-size: 16px; margin: 0 0 4px; }} h3 {{ font-size: 13px; color: #52514e; margin: 16px 0 4px; }}
p {{ color: #52514e; max-width: 72em; }}
svg text {{ font-size: 11px; font-variant-numeric: tabular-nums; fill: #0b0b0b; }}
svg text.row {{ font-family: ui-monospace, monospace; }}
svg text.group {{ font-weight: 600; }}
svg text.size {{ fill: #898781; }}
svg .sep {{ stroke: #c3c2b7; stroke-width: 1; }}
.legend span {{ display: inline-flex; align-items: center; gap: 4px; margin-right: 14px; font-size: 12px; }}
.legend i {{ width: 14px; height: 14px; border-radius: 3px; display: inline-block; }}
</style></head><body>
<h1>{title}</h1>
{intro}
{legend}
{sections}
</body></html>
"""

GROUPS = [("all", ["all"]), ("workload", list(SCOPES["workload"])[1:]), ("size n", list(SCOPES["n"])[1:]),
          ("key", list(SCOPES["elem"])[1:])]
ALL_SCOPES = {k: v for scopes in SCOPES.values() for k, v in scopes.items()}


def effect_rows(directory, kind, factors, fixed, interactions):
    runs = screening() if kind == "screening" else full(factors, fixed, kind == "half")
    values = [opts(run) for run in runs]
    varied = [f for f in FACTORS if len({run[f] for run in runs}) == 2]
    mains = [(f,) for f in varied]
    chains = alias_chains(values, list(itertools.combinations(varied, 2))) if interactions else []
    table = effects_table(directory, values, mains + [rep for rep, _ in chains], ALL_SCOPES)
    rows = [(term_name(t), table[t]) for t in mains]
    for rep, members in chains:
        rows.append((" = ".join(("" if sign > 0 else "−") + term_name(t) for sign, t in members), table[rep]))
    return rows


def combination_rows(directory, only=None):
    cells = load(directory)
    rows = []
    for value in sorted({v for by in cells.values() for v in by} - {0}):
        if only is not None and value not in only:
            continue
        per_cell = {key: by[value] - by[0] for key, by in cells.items() if value in by and 0 in by}
        on = "+".join(f for f, bit in FACTORS.items() if value & bit and f != "dummy") or "none"
        rows.append((f"{on}{' +dummy' if value & 32 else ''}",
                     {scope: summarize(per_cell, select) for scope, select in ALL_SCOPES.items()}))
    return sorted(rows, key=lambda row: row[1]["all"]["change"])


def predict_rows(directory, kind, factors, fixed):
    """Every combination of the varied factors against all off, from main effects and two-factor interactions.

    With orthogonal two-level columns each term's coefficient is half its effect, so the predicted log ratio
    of a combination is the sum over terms of effect / 2 times the change of the term's level.
    """
    runs = screening() if kind == "screening" else full(factors, fixed, kind == "half")
    design = [opts(run) for run in runs]
    varied = [f for f in FACTORS if len({run[f] for run in runs}) == 2]
    terms = [(f,) for f in varied] + list(itertools.combinations(varied, 2))
    per_term = contrasts(load(directory), design, terms)
    rows = []
    for on in itertools.product((False, True), repeat=len(varied)):
        level = {f: (1 if bit else -1) for f, bit in zip(varied, on)}
        weights = [(term, (math.prod(level[f] for f in term) - (-1) ** len(term)) / 2) for term in terms]
        per_cell = {key: sum(w * per_term[term][key] for term, w in weights if w)
                    for key in per_term[terms[0]]}
        label = "+".join(f for f, bit in zip(varied, on) if bit) or "none"
        rows.append((label, {scope: summarize(per_cell, select) for scope, select in ALL_SCOPES.items()}))
    return sorted(rows, key=lambda row: row[1]["all"]["change"])


def parse_factors(text):
    return [f.strip() for f in text.split(",") if f.strip()]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    design = sub.add_parser("design", help="print the Opts values of a design")
    design.add_argument("kind", choices=("screening", "full", "half"))
    design.add_argument("--factors", type=parse_factors, default=[])
    design.add_argument("--fixed", type=int, default=0, help="Opts bits held on for factors not varied")
    registry = sub.add_parser("registry", help="print C++ registry lines for Opts values not yet named")
    registry.add_argument("opts", type=int, nargs="+")
    effects = sub.add_parser("effects", help="estimate factor effects from one language's passes")
    effects.add_argument("directory")
    effects.add_argument("kind", choices=("screening", "full", "half"))
    effects.add_argument("--factors", type=parse_factors, default=[])
    effects.add_argument("--fixed", type=int, default=0)
    effects.add_argument("--interactions", action="store_true", help="add two-factor interaction alias chains")
    effects.add_argument("--by", choices=SCOPES, default="workload", help="table columns")
    effects.add_argument("--json", action="store_true")
    names = sub.add_parser("names", help="print C++ heap names for comma-separated Opts values")
    names.add_argument("opts")
    shuffle = sub.add_parser("shuffle", help="print a comma list in an order seeded by PASS")
    shuffle.add_argument("seed", type=int)
    shuffle.add_argument("items")
    combos = sub.add_parser("combinations", help="each measured Opts value against Opts 0")
    combos.add_argument("directory")
    predict = sub.add_parser("predict", help="rank all combinations by a main plus two-factor interaction model")
    predict.add_argument("directory")
    predict.add_argument("kind", choices=("screening", "full", "half"))
    predict.add_argument("--factors", type=parse_factors, default=[])
    predict.add_argument("--fixed", type=int, default=0)
    predict.add_argument("--top", type=int, default=12)
    predict.add_argument("--by", choices=SCOPES, default="workload", help="table columns")
    report = sub.add_parser("report", help="write an HTML page from a JSON list of sections")
    report.add_argument("spec", help='JSON file: {"title", "intro", "sections": [{"heading", "text", "directory", '
                                     '"kind": "screening"|"full"|"half"|"combinations"|"predict", "design", "factors", "fixed", "only", "top"}]}')
    report.add_argument("output")
    args = parser.parse_args()

    if args.command in ("design", "effects"):
        runs = screening() if args.kind == "screening" else full(args.factors, args.fixed, args.kind == "half")
        values = [opts(run) for run in runs]
        if args.command == "design":
            print(f"# {len(runs)} runs, resolution {resolution(runs)}")
            print(",".join(map(str, values)))
            return
        varied = [f for f in FACTORS if len({run[f] for run in runs}) == 2]
        mains = [(f,) for f in varied]
        chains = alias_chains(values, list(itertools.combinations(varied, 2))) if args.interactions else []
        table = effects_table(args.directory, values, mains + [rep for rep, _ in chains], SCOPES[args.by])
        if args.json:
            print(json.dumps({term_name(t): v for t, v in table.items()}, indent=1))
            return
        print(f"{args.directory}: change in % when the term is on, 95% interval over passes")
        rows = [(term_name(t), table[t]) for t in mains]
        for rep, members in chains:
            label = " ".join(("" if sign > 0 else "-") + term_name(t) for sign, t in members)
            rows.append((label if len(label) < 40 else label[:37] + "...", table[rep]))
        print_table(rows)
    elif args.command == "registry":
        for value in args.opts:
            if value not in NAMED.values():
                print(f'    QuickHeapVariant{{"{heap_name(value)}", {value}}},')
    elif args.command == "names":
        print(",".join(heap_name(int(v)) for v in args.opts.split(",")))
    elif args.command == "shuffle":
        items = args.items.split(",")
        random.Random(args.seed).shuffle(items)
        print(",".join(items))
    elif args.command == "report":
        spec = json.loads(Path(args.spec).read_text(encoding="utf-8"))
        sections = []
        for part in spec["sections"]:
            if part["kind"] == "combinations":
                rows = combination_rows(part["directory"], part.get("only"))
            elif part["kind"] == "predict":
                rows = predict_rows(part["directory"], part["design"], part.get("factors", []), part.get("fixed", 0))
                rows = rows[:part.get("top", len(rows))]
            else:
                rows = effect_rows(part["directory"], part["kind"], part.get("factors", []), part.get("fixed", 0),
                                   part.get("interactions", True))
            sections.append(f'<section><h2>{html.escape(part["heading"])}</h2><p>{part.get("text", "")}</p>'
                            f'{heatmap(part["heading"], rows, GROUPS)}</section>')
        Path(args.output).write_text(PAGE.format(title=html.escape(spec["title"]), intro=spec.get("intro", ""),
                                                 legend=legend(), sections="\n".join(sections)), encoding="utf-8")
    elif args.command == "predict":
        rows = predict_rows(args.directory, args.kind, args.factors, args.fixed)[:args.top]
        print(f"{args.directory}: predicted change in % against all off, 95% interval over passes")
        print_table([(label, {k: by[k] for k in SCOPES[args.by]}) for label, by in rows])
    elif args.command == "combinations":
        print(json.dumps({str(k): v for k, v in sorted(combinations(args.directory).items())}, indent=1))


if __name__ == "__main__":
    main()
