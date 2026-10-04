"""Compare stored JSON values without depending on serializer formatting."""
import argparse
import json
from pathlib import Path


def same(a, b):
    if isinstance(a, bool) or isinstance(b, bool):
        return type(a) is type(b) and a == b
    if isinstance(a, (float, int)) and isinstance(b, (float, int)):
        return a == b
    if type(a) is not type(b):
        return False
    if isinstance(a, dict):
        return a.keys() == b.keys() and all(same(a[k], b[k]) for k in a)
    if isinstance(a, list):
        return len(a) == len(b) and all(same(x, y) for x, y in zip(a, b))
    return a == b


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--left', type=Path, required=True)
    ap.add_argument('--right', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()
    assert not args.output.exists(), 'Refusing to overwrite comparison'
    equal = same(json.loads(args.left.read_text(encoding='utf-8-sig')),
                 json.loads(args.right.read_text(encoding='utf-8-sig')))
    result = {'values_equal': equal, 'device_requests': 0,
              'left': str(args.left), 'right': str(args.right),
              'numeric_formatting_ignored': True, 'bool_number_types_distinct': True}
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))
    if not equal:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
