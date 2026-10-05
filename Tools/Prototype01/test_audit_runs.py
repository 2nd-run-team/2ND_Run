"""작성자 : 임진혁. 정산 검사기의 실제 회귀 위험만 검사한다."""
import copy,unittest
from audit_runs import audit
class LedgerAuditTests(unittest.TestCase):
 def rows(self,phase='Succeeded'):
  common={'RunId':'run','schemaVersion':2,'buildId':'build','settingsVersion':'v1','layoutVariant':'A','playerId':1,'CarId':'P01_C01','CargoId':'None','playerCount':2,'teamValue':0,'value':0,'phase':'Running'}
  specs=[{'event':'RunStart','remainingByCar':{'P01_C01':40}},{'event':'CarEnter','remainingCarValue':40},{'event':'AcquireComplete','CargoId':'cargo1','value':40,'teamValue':40},{'event':'CarExit','teamValue':40,'remainingCarValue':0},{'event':'RunEnd','teamValue':40,'value':40 if phase=='Succeeded' else 0,'phase':phase,'escaped':1 if phase=='Succeeded' else 0,'leftBehind':1 if phase=='Succeeded' else 2,'remainingByCar':{'P01_C01':0}}]
  return [{**common,**row,'sequence':i+1,'serverTime':i} for i,row in enumerate(specs)]
 def test_success_and_failure_keep_transmitted_ledger(self):
  for phase in ['Succeeded','Failed','Aborted']:
   report=audit(self.rows(phase));self.assertTrue(report['passed'],report);self.assertEqual(report['transmitted_value'],40)
 def test_duplicate_completion_cannot_inflate_result(self):
  rows=self.rows();rows.insert(3,copy.deepcopy(rows[2]))
  for i,row in enumerate(rows):row['sequence']=i+1;row['serverTime']=i
  result=audit(rows);self.assertFalse(result['passed']);self.assertIn('duplicate CargoId cargo1',result['errors'])
 def test_duplicate_result_is_rejected(self):
  rows=self.rows();rows.append(copy.deepcopy(rows[-1]));rows[-1].update(sequence=6,serverTime=5)
  result=audit(rows);self.assertFalse(result['passed']);self.assertIn('expected one RunEnd',result['errors'])
 def test_failed_run_cannot_pay_transmitted_value(self):
  rows=self.rows('Failed');rows[-1]['value']=40;self.assertFalse(audit(rows)['passed'])
 def test_incomplete_records_are_not_settlements(self):
  rows=self.rows('Running');self.assertIn('RunEnd is not terminal',audit(rows)['errors'])
  rows=self.rows();rows[2]['CargoId']='None';self.assertIn('missing CargoId on completion',audit(rows)['errors'])
 def test_missing_value_or_mixed_config_is_rejected(self):
  for field,value in [('teamValue',39),('settingsVersion','v2')]:
   rows=self.rows();rows[-1][field]=value;self.assertFalse(audit(rows)['passed'])
 def test_first_departure_and_final_remaining_are_separate(self):
  report=audit(self.rows());car=report['cars']['P01_C01'];self.assertEqual(car['first_team_empty_remaining'],0);self.assertEqual(car['team_first_to_last_seconds'],2)
if __name__=='__main__':unittest.main()
