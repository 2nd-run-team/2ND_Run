# 플레이어 상태와 구조 테스트

`USPPlayerStatusComponent`는 플레이어의 체력, 피해, 다운, 구조를 관리한다. 이후 스태미너도 이 컴포넌트에 추가할 수 있다. 현재 스태미너 소모와 회복은 구현 범위에 포함하지 않는다.

## 실행과 조작

Editor 타깃을 빌드한 뒤 `Lvl_SPTestMap`에서 Listen Server, 플레이어 2명으로 실행한다. `ASPPlayerCharacter`를 상속하는 기존 플레이어 BP에는 `Status`와 `ReviveInteraction`이 자동으로 추가된다. 각 플레이어 화면 왼쪽 아래에 `HP 현재 / 최대`, 다운 여부와 활동 인원 수가 표시된다.

| 조작 | 결과 |
| --- | --- |
| F6 | 자신에게 피해 25 적용 |
| F7 | 자신을 다운 |
| 동료를 바라보고 E 유지 | 4초 후 동료를 최대 체력의 30%로 구조 |
| Shift+F7 | 호스트가 모든 플레이어의 체력과 작전 실패 상태 초기화 |

테스트 키는 Development에서만 동작하며 `Enable Status Debug Controls`로 끌 수 있다. 초기화는 체력과 작전 실패 상태만 초기화한다. 가방, 경비의 신원 기억, 레벨 배치를 되돌리려면 PIE를 다시 시작한다.

다운 시 이동과 상호작용, 물건 조작이 중단되고 등 가방만 떨어진다. 키카드와 도구는 유지한다. 구조자가 E를 놓거나 시작 위치에서 5cm보다 멀리 이동하거나 다운되면 구조를 취소한다. 거리 이탈과 벽에 의한 차단도 취소 조건이다. 구조 진행은 보존하지 않으며 한 명만 구조할 수 있다.

다운 상태에서도 시점 회전은 가능하다. 구조 후 위장 발각은 유지한다. 전원 다운이면 복제된 작전 실패 상태와 화면 안내가 켜지고 초기화 전까지 유지된다. 정산 및 스테이지 전환은 이후 작전 흐름에서 `OnOperationFailed`에 연결한다.

## 블루프린트에서 조정

플레이어 BP의 `Status` 컴포넌트에서 `Health`와 `Revive` 항목을 조정한다. 기본값은 최대 체력 100, 구조 4초, 회복 비율 0.3, 이동 허용 오차 5cm이다.

- 서버의 일반 `Apply Damage`를 사용하면 상태 컴포넌트가 피해를 받는다. 컴포넌트의 `ApplyDamage`와 `ResetForStage`도 서버 전용이다.
- `GetStatusComponent`, 체력 조회 함수, `IsDowned`를 통해 상태를 읽는다. 도구 등 새 행동을 추가할 때 서버에서도 다운 여부를 확인한다.
- `OnHealthChanged`, `OnLifeStateChanged`에 UI와 표현을 연결한다. `LifeState`는 상태 컴포넌트 안에서 활동/다운을 나타내는 하위 상태다.
- 숫자 HUD는 `USPPlayerStatusHUDWidget`의 기본 UMG 레이아웃이다. Widget BP를 상속해 플레이어의 `Status HUD Widget Class`에 지정할 수 있다. `HealthText`, `StateText`, `RescueText`, `TeamText`, `HintText`, `FailureText`라는 TextBlock을 두면 기본 표시 로직을 사용한다. `Show Status Debug HUD`로 표시를 끈다.
- 임시 다운 자세는 서버에서 루트 캡슐을 옆으로 회전시켜 메시와 구조 판정 영역을 함께 눕힌다. 지상에서는 캡슐 높이 차이만큼 충돌 검사하며 내리고, 구조 후 기존 중력 복구 경로가 캡슐을 다시 세운다. 회전 경로가 막히면 반대쪽을 시도하며 양쪽 모두 막히면 현재 방향을 유지한다. 무중력에서 구조되면 기존 무중력 회전 경로로 시선을 따라 복귀한다.
- 애니메이션은 이동용 루트 캡슐을 자동으로 눕히지 않는다. 정식 다운 애니메이션을 도입할 때는 `Use Temporary Down Pose`와 플레이어의 `On Status State Changed` 이벤트를 이용해 캡슐·메시 처리를 함께 설계한다.

## 코드의 역할

생존 규칙과 복제 데이터는 `SPPlayerStatusComponent`, 입력과 이동·가방·표현 연결은 `SPPlayerCharacter`, E 시간 측정은 기존 상호작용 컴포넌트가 담당한다. `SpacePirateGameMode`는 플레이어 컨트롤러가 조종하는 인원을 집계하고, `SPGameState`는 활동 인원과 작전 실패를 모든 클라이언트에 전달한다. NPC 경비는 활동 인원에 포함하지 않는다.

## 검증 실행

자동화 필터 `SpacePirate.PlayerStatus`는 피해, 다운 제한, 동시 구조, 구조 취소와 스테이지 초기화를 검사한다. `SpacePirate` 전체 필터는 기존 이동·인벤토리·상호작용·잠입 회귀 검사도 실행한다.

`Tools/PlayerStatus/run_pie_verification.py`는 Play Net Mode를 Listen Server로 설정한 뒤 별도 에디터를 실행할 때 `-ExecutePythonScript` 인수로 지정한다. 2인 PIE에서 실제 E 입력과 상태 복제, 양쪽 숫자 HUD를 확인하고 `Saved/PlayerStatus/pie-verification.json`을 기록한 뒤 종료한다. 에셋을 저장하지 않으며 플레이 중인 기존 에디터에서 실행하지 않는다.

임시 HUD의 Slate 스타일을 위해 `SlateCore` 모듈 의존성이 추가됐다. 로컬 검증 엔진은 설치된 UE 5.8.3이며 저장소의 팀 기준 버전 표기는 수정하지 않았다.

### 2026년 10월 8일 검증 기록

Development Editor 빌드와 프로젝트 파일 재생성을 완료했다. 기존 자동 검사 17개와 플레이어 상태 검사 5개가 통과했다. 상태 검사는 웅크린 상태에서 다운·구조 후 캡슐, 카메라, 메시와 네트워크 보정 기준의 복구도 확인한다. 메시 없는 네이티브 테스트 캐릭터에서 소켓 조회 경고가 발생하며 실패 항목은 없다.

추가로 2인 Listen Server PIE 검사 27개가 모두 통과했다. 호스트와 클라이언트 양쪽에서 E로 상대를 구조하고, 키 해제·이동 시 진행 초기화, 4초 후 체력 30 복구, 전원 다운과 스테이지 초기화 복제까지 확인했다. 각 소유 플레이어의 HUD 숫자·상태·실패 안내를 검사하고 실제 HUD 스크린샷을 확인했다. 구조 회복값의 부동소수점 오차로 30이 31로 보이던 표시도 수정했다.

PIE 결과는 `Saved/PlayerStatus/pie-verification.json`, 화면은 같은 폴더의 `hud-initial.png`와 `hud-downed.png`에 있다. 이번 네트워크 검증은 단일 프로세스의 2인 PIE이며 별도 PC 간 지연·패킷 손실 조건은 포함하지 않는다.

### 2026년 10월 8일 다운 캡슐 수정 검증

기존 임시 자세는 메시만 눕혀 보이는 몸과 구조 판정용 캡슐이 어긋났다. 캡슐을 서버에서 회전시키고 기존 위치·회전 복제로 전달하도록 변경했다. 메시의 상대 변환을 별도로 저장·복구하던 코드는 제거했다. 이때 네트워크 보정 중인 값을 기준값으로 다시 저장하면 원격 화면에서 몸이 서 있는 문제가 생기므로, 메시 보정은 기존 이동 컴포넌트가 관리한다.

최종 Development Editor 빌드, 전체 자동 검사 23개, 2인 PIE 검사 35개가 통과했다. 빈손 상태에서 머리와 몸통을 조준한 구조, `BP_SPSmallLoot`를 손에 든 상태에서 머리를 조준한 구조, 서버·소유 클라이언트·원격 화면의 캡슐/몸 방향, 구조 후 직립, 즉시 초기화 후 반복 다운을 확인했다. 화물 줍기 우선순위와 화물 충돌 코드는 이번 수정에서 바꾸지 않았다.

자동 검사 결과는 `Saved/PlayerStatus/CapsuleAutomation/index.json`, PIE 결과는 `Saved/PlayerStatus/pie-verification.json`, 화물 운반 중 구조 화면은 `Saved/PlayerStatus/revive-capsule-cargo.png`에 있다. 양쪽 회전 경로가 모두 막힌 좁은 공간에서는 현재 캡슐 방향을 유지하는 임시 처리다.
