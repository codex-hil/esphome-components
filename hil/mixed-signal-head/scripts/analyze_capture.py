"""Extract threshold crossings from saved analog captures; no instrument I/O."""
import argparse
import json
from pathlib import Path
import statistics

p = argparse.ArgumentParser()
p.add_argument('name')
p.add_argument('--threshold', type=float, default=1.65)
p.add_argument('--center-us', type=float, default=0)
p.add_argument('--span-us', type=float, default=2)
p.add_argument('--view', default='boundary')
a = p.parse_args()
root = Path(__file__).resolve().parents[1] / 'results'
if Path(a.name).name != a.name or Path(a.view).name != a.view or a.span_us <= 0:
    p.error('name must be a filename stem')
meta = json.loads((root / (a.name + '.json')).read_text())
result = {'capture': a.name, 'threshold_V': a.threshold,
          'uncertainty': 'Sample interval recorded; probe/channel skew and analog accuracy not calibrated.',
          'channels': {}}
traces = {}
for ch, data in meta['channels'].items():
    q = data['preamble']
    raw = (root / f'{a.name}-ch{ch}.bin').read_bytes()
    if len(raw) != data['n']:
        raise ValueError('Sample count mismatch')
    volts = [((b if b < 128 else b-256)-q['yoff'])*q['ymult']+q['yzero'] for b in raw]
    t0 = q['xzero']-q['pt_off']*q['dt_s']
    edges = []
    for i in range(1, len(volts)):
        if (volts[i] >= a.threshold) != (volts[i-1] >= a.threshold):
            fraction = (a.threshold-volts[i-1])/(volts[i]-volts[i-1])
            edges.append({'time_s': t0+(i-1+fraction)*q['dt_s'],
                          'kind': 'rise' if volts[i] >= a.threshold else 'fall'})
    widths = [(b['time_s']-x['time_s']) for x, b in zip(edges, edges[1:])
              if x['kind'] == 'rise' and b['kind'] == 'fall']
    result['channels'][ch] = {'edges': edges, 'sample_interval_s': q['dt_s'],
                               'high_widths_s': widths,
                               'median_high_width_s': statistics.median(widths) if widths else None}
    traces[ch] = (t0, q['dt_s'], volts)
sck = result['channels']['1']['edges']
header = next((e['time_s'] for e in result['channels']['4']['edges'] if e['kind'] == 'rise'), None) \
    if meta['probe_map'].get('4') == 'Header_done' else None
cs = next((e['time_s'] for e in result['channels']['2']['edges'] if e['kind'] == 'fall'), None)
if header is not None:
    result['sck_rises_before_header_done'] = sum(e['kind'] == 'rise' and e['time_s'] < header for e in sck)
if cs is not None and header is not None:
    result['cs_fall_minus_header_done_s'] = cs-header
    result['sck_rises_before_cs_fall'] = sum(e['kind'] == 'rise' and e['time_s'] < cs for e in sck)
    first_payload = next((e['time_s'] for e in sck if e['kind'] == 'rise' and e['time_s'] > cs), None)
    result['first_sck_rise_after_cs_minus_cs_s'] = first_payload-cs if first_payload else None
(root / f'{a.name}-edges.json').write_text(json.dumps(result, indent=2)+'\n')

# Standalone SVG: min/max per pixel preserves narrow transients when zooming out.
width, height = 1100, 700
left, right = 100, 1060
start, stop = (a.center_us-a.span_us/2)*1e-6, (a.center_us+a.span_us/2)*1e-6
parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
         '<rect width="100%" height="100%" fill="white"/>',
         f'<text x="100" y="25" font-family="sans-serif" font-size="18">{a.name}: {a.center_us-a.span_us/2:g}…{a.center_us+a.span_us/2:g} µs</text>']
colors = ['#2563eb', '#dc2626', '#059669', '#7c3aed']
for index, (ch, (t0, dt, volts)) in enumerate(traces.items()):
    top = 55+index*155
    label = meta['probe_map'][ch]
    parts.append(f'<text x="10" y="{top+60}" font-family="sans-serif" font-size="12">CH{ch} {label}</text>')
    for value in (0, 1.65, 3.3):
        y = top+120-value*30
        parts.append(f'<path d="M{left},{y}H{right}" stroke="#ddd"/>')
    bins = [[] for _ in range(right-left)]
    for i, v in enumerate(volts):
        t = t0+i*dt
        if start <= t < stop:
            bins[min(len(bins)-1, int((t-start)/(stop-start)*len(bins)))].append(v)
    path = []
    for i, values in enumerate(bins):
        if values:
            x = left+i
            path.append(f'M{x},{top+120-min(values)*30:.2f}V{top+120-max(values)*30:.2f}')
    parts.append(f'<path d="{" ".join(path)}" stroke="{colors[index]}" fill="none"/>')
    zero = left+(right-left)/2
    parts.append(f'<path d="M{zero},{top}V{top+135}" stroke="#777" stroke-dasharray="3 3"/>')
parts.append('<text x="100" y="690" font-family="sans-serif" font-size="12">Saved analog samples; 0 V / 1.65 V / 3.3 V grid. Probe/channel skew not calibrated.</text></svg>')
(root / f'{a.name}-{a.view}.svg').write_text('\n'.join(parts))
print(json.dumps({k:v for k,v in result.items() if k != 'channels'}, indent=2))
print('SCK median HIGH ns:', (result['channels']['1']['median_high_width_s'] or 0)*1e9)
