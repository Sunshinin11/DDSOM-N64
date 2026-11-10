from pathlib import Path
import re,json
p=Path(__file__).resolve().parent;b=p/'build'
labels=['baseline','native-driver','silent','recorded','gpu','poll','cacheoff','save','cache-restored','renderer-Os','renderer-O2','final-play']
summary={}
for label in labels:
 f=b/f'five-{label}.log'
 if not f.exists():continue
 text=f.read_text(errors='replace');rows=[]
 for m in re.finditer(r'\[DD_SOM_PERF\] frames=(\d+) fps100=(\d+).*?logic_us=(\d+) scene_walk_us=(\d+).*?tile_misses=(\d+).*?interp_total=(\d+)',text):
  rows.append(dict(frame=int(m[1]),fps=int(m[2])/100,logic_us=int(m[3]),scene_us=int(m[4]),tile_misses=int(m[5]),interpreted=int(m[6])))
 compare=[r for r in rows if r['frame'] in (480,540,600,660)]
 summary[label]={'log':f.name,'matched_windows':compare,'sample_min_fps':min((r['fps'] for r in rows),default=None),'sample_max_fps':max((r['fps'] for r in rows),default=None),'matched_fps':len(compare)*60/sum(60/r['fps'] for r in compare) if compare else None,'completed':'DDSOM_BENCH_DONE' in text,'failures':bool(re.search(r'ASSERTION FAILED|DDSOM_UNTRANSLATED|DDSOM_FATAL',text))}
(b/'five-benchmark-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
lines=['| Versão | Quadro 480 | 540 | 600 | 660 |','|---|---:|---:|---:|---:|']
for label,v in summary.items():
 values={r['frame']:f"{r['fps']:.2f}" for r in v['matched_windows']}
 lines.append('| '+label+' | '+' | '.join(values.get(frame,'—') for frame in (480,540,600,660))+' |')
(b/'five-benchmark-table.md').write_text('\n'.join(lines)+'\n')
print('\n'.join(lines))

