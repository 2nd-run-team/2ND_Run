"""작성자 : 임진혁
P5 서버 JSONL의 완료 원장/정산/방문 기록 검사. 실패 판의 정산은 0, 전송 합계는 유지한다.
사용: python Tools/Prototype01/audit_runs.py Saved/Prototype01/Runs/<RunId>.jsonl --out Saved/Prototype01/audit.json
"""
import argparse,collections,json
from pathlib import Path

def audit(rows,allow_open=False):
 errors=[]
 def require(ok,message):
  if not ok:errors.append(message)
 require(bool(rows),'empty log')
 if not rows:return {'passed':False,'errors':errors}
 require(len({r.get('RunId') for r in rows})==1,'mixed RunIds')
 require(all(r.get('schemaVersion')==2 for r in rows),'requires P5 schemaVersion 2')
 for key in ['buildId','settingsVersion','layoutVariant']:
  require(all(r.get(key) for r in rows),'missing '+key)
  require(len({r.get(key) for r in rows})==1,'changed '+key)
 sequence=[r.get('sequence') for r in rows]
 require(all(isinstance(n,int) for n in sequence) and sequence==list(range(1,len(rows)+1)),'duplicate, missing or unordered sequence')
 require(all(rows[i]['serverTime']>=rows[i-1]['serverTime'] for i in range(1,len(rows))),'server time moved backwards')
 starts=[r for r in rows if r['event']=='RunStart'];ends=[r for r in rows if r['event']=='RunEnd']
 require(len(starts)==1,'expected one RunStart')
 require(len(ends)==1 or (allow_open and not ends),'expected one RunEnd')
 for event in ['ExtractionStart','MaintenanceStart']:require(sum(r['event']==event for r in rows)<=1,'duplicate '+event)
 credited={};value=0;by_car=collections.Counter();finished=False;started=False
 visits={};individual=collections.defaultdict(list);active=collections.defaultdict(set);team=collections.defaultdict(list);team_begin={};first_player_exit={};first_team_exit={}
 for r in rows:
  event=r['event'];car=r.get('CarId','None');player=r.get('playerId',-1);time=r['serverTime']
  if event=='RunStart':started=True
  if event=='AcquireComplete':
   require(started and not finished,'completion outside running round')
   cargo=r.get('CargoId');amount=r.get('value')
   require(isinstance(cargo,str) and cargo not in ['', 'None'],'missing CargoId on completion')
   require(cargo not in credited,'duplicate CargoId '+str(cargo))
   require(isinstance(amount,int) and amount>=0,'invalid cargo value')
   if isinstance(amount,int):value+=amount;by_car[car]+=amount
   credited[cargo]=amount
  if started:require(r.get('teamValue')==value,'teamValue mismatch at sequence '+str(r.get('sequence')))
  if event=='RunEnd':
   require(r['phase'] in ['Succeeded','Failed','Aborted'],'RunEnd is not terminal')
   expected=value if r['phase']=='Succeeded' else 0
   require(r.get('value')==expected,'final settlement mismatch')
   require(r.get('escaped',0)>0 if r['phase']=='Succeeded' else r.get('escaped',0)==0,'escape count contradicts result')
   require(r.get('escaped',0)+r.get('leftBehind',0)==r.get('playerCount'),'result player count mismatch')
   finished=True
  key=(player,car)
  if event=='CarEnter':
   require(key not in visits,'duplicate CarEnter')
   visits[key]=time
   if not active[car]:team_begin[car]=time
   active[car].add(player)
  elif event=='CarExit':
   require(key in visits,'CarExit without enter')
   if key in visits:individual[car].append({'playerId':player,'enter':visits.pop(key),'exit':time})
   first_player_exit.setdefault(car,r.get('remainingCarValue'))
   active[car].discard(player)
   if not active[car] and car in team_begin:
    team[car].append({'first_enter':team_begin.pop(car),'last_exit':time})
    first_team_exit.setdefault(car,r.get('remainingCarValue'))
 if ends:
  require(not visits,'open car visit after RunEnd')
  initial=starts[0].get('remainingByCar',{}) if starts else {};remaining=ends[0].get('remainingByCar',{})
  for car,total in initial.items():require(total==by_car[car]+remaining.get(car,0),'car budget mismatch '+car)
 else:remaining={}
 cars={}
 for car in set(individual)|set(team)|set(remaining):
  intervals=team[car];span=intervals[-1]['last_exit']-intervals[0]['first_enter'] if intervals else 0
  cars[car]={'individual_visits':individual[car],'team_occupied_intervals':intervals,'team_first_to_last_seconds':span,'team_occupied_seconds':sum(x['last_exit']-x['first_enter'] for x in intervals),'first_player_exit_remaining':first_player_exit.get(car),'first_team_empty_remaining':first_team_exit.get(car),'end_remaining':remaining.get(car)}
 return {'passed':not errors,'errors':errors,'RunId':rows[0].get('RunId'),'buildId':rows[0].get('buildId'),'settingsVersion':rows[0].get('settingsVersion'),'layoutVariant':rows[0].get('layoutVariant'),'completions':credited,'transmitted_value':value,'final_value':ends[0]['value'] if len(ends)==1 else None,'phase':ends[0]['phase'] if len(ends)==1 else 'Open','cars':cars}

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('logs',nargs='+',type=Path);p.add_argument('--out',type=Path);p.add_argument('--allow-open',action='store_true');args=p.parse_args()
 result=[]
 for path in args.logs:
  try:rows=[json.loads(s) for s in path.read_text(encoding='utf-8-sig').splitlines() if s.strip()];report=audit(rows,args.allow_open)
  except (OSError,ValueError,KeyError,TypeError) as e:report={'passed':False,'errors':[str(e)]}
  result.append({'file':str(path),**report})
 output={'passed':all(r['passed'] for r in result),'runs':result};text=json.dumps(output,ensure_ascii=False,indent=2)
 if args.out:args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(text+'\n',encoding='utf8')
 print(json.dumps({'passed':output['passed'],'runs':len(result),'errors':[r['errors'] for r in result]},ensure_ascii=True))
 raise SystemExit(0 if output['passed'] else 1)
if __name__=='__main__':main()
