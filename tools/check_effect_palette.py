"""Check actual before/after RGBA from the optional native palette fixture.

Rows are opaque effects, half-coverage effects and unchanged opaque controls.
This checks displayed palette values through the native shader/MSAA/readback;
it does not qualify gameplay appearance or performance.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

WIDTH, HEIGHT = 512, 384
PALETTES = (("capture-ring", (.5, 1, .76)), ("capture-trail", (.29, .96, .71)),
            ("warning", (1, .28, .23)), ("missile-trail", (1, .24, .13)),
            ("laser-core", (.55, .93, 1)), ("low-values", (.02, .04, .07)))


def analyze(before: bytes, after: bytes) -> dict:
    if len(before) != WIDTH * HEIGHT * 4 or len(after) != len(before):
        raise ValueError("Require one 512x384 RGBA frame from each native fixture")

    def sample(data, row, column):
        x, y = int((column + .5) * WIDTH / 6), int((row + .5) * HEIGHT / 3)
        offset = (y * WIDTH + x) * 4
        return list(data[offset:offset + 3])

    cases, failures = [], []
    for column, (name, palette) in enumerate(PALETTES):
        old, actual = sample(before, 0, column), sample(after, 0, column)
        expected = [round(value * 255) for value in palette]
        error = max(abs(a - b) for a, b in zip(actual, expected))
        # Two cube faces overlap in the translucent row with depth writes off.
        # Its expected accumulated coverage is 1-(1-.5)^2 = .75.
        expected_alpha = [round((value * .75 + background * .25) * 255)
                          for value, background in zip(palette, (.5, .55, .4))]
        alpha = sample(after, 1, column)
        alpha_error = max(abs(a - b) for a, b in zip(alpha, expected_alpha))
        control_unchanged = sample(before, 2, column) == sample(after, 2, column)
        if error > 1 or alpha_error > 2 or not control_unchanged:
            failures.append(name)
        cases.append(dict(name=name, authored_srgb=list(palette), before_rgb8=old,
                          after_rgb8=actual, expected_rgb8=expected, max_rgb8_error=error,
                          alpha_rgb8=alpha, expected_alpha_rgb8=expected_alpha,
                          alpha_max_rgb8_error=alpha_error, opaque_control_unchanged=control_unchanged))
    # Check every pixel in the lit control band, including AA silhouette edges.
    band = slice(WIDTH * 256 * 4, WIDTH * HEIGHT * 4)
    controls_identical = before[band] == after[band]
    return dict(schema=1, scope="native GPU palette fixture; not gameplay/performance qualification",
                cases=cases, opaque_control_band_identical=controls_identical,
                before_sha256=hashlib.sha256(before).hexdigest(),
                after_sha256=hashlib.sha256(after).hexdigest(),
                passed=not failures and controls_identical, failures=failures)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    report = analyze(args.before.read_bytes(), args.after.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
