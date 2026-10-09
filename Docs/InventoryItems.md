# 인벤토리·아이템

담당 HolicKW. 브랜치 `feature/item-interaction`.

## 2026-10-09 추가: 키카드 보안문, 일반 보관함, 특수 금고

### 추가한 것

| 대상 | 클래스 / BP | 동작 |
|---|---|---|
| 열기 대상 공통 | `ASPOpenable` | E로 열면 문짝(`DoorMesh`)이 `OpenOffset`만큼 비켜나고 계속 열린다. 닫힌 문짝이 E 트레이스를 막아 안의 물건과 묶음은 열기 전에 조작할 수 없다 |
| 키카드 보안문 | `BP_SPSecurityDoor` (부모 `SPOpenable`) | 키카드를 **손에 들고** E를 짧게 누르면 열린다. 카드는 소모하지 않는다 |
| 일반 보관함 | `BP_SPLocker` (부모 `SPOpenable`) | E를 길게 눌러 열고 안의 소형 전리품을 따로 줍는다. `ContentsBox` 안에 놓인 물건은 처음 주울 때만 범죄다 |
| 특수 금고 | `ASPVault` / `BP_SPVault` | 직접 해제(누적 30초, 진행률 보존, 이동·시점 잠금)와 드릴 설치(3초). 드릴 가방을 멘 사람이 E를 누르면 설치가 골라진다. 설치하면 가방이 사라지고, 직접 해제한 만큼 덜 작업한 뒤 연다. 금고 전체에서 한 명만 조작한다 |
| E 거절 안내 | `USPInteractableComponent::BlockedPrompt`, `USPInteractorComponent::GetBlockedMessage` | E가 거절되면 이유(키카드 필요, 드릴 작동 중 · 남은 N초)를 1.5초 보여 준다 |

소형 금고와 화물 상자도 `SPOpenable`의 BP로 만들 수 있다(여는 시간과 범죄 종류만 다르다). 아직 BP는 없다.

### BP 설정

| BP | 설정 |
|---|---|
| 보안문 | `bRequiresHandItem` 켜기, `RequiredHandItem` = Keycard. Interactable: HoldDuration 0, BlockedPrompt "키카드 필요", CrimeKind `KeycardDoor`, `bInstantCrime` 켜기 |
| 보관함 | `ContentsBox`를 안쪽 크기에 맞춤(Box Extent는 절반 크기), `bIsClueWhenOpen` 켜기. Interactable: HoldDuration 시험값, CrimeKind `ContainerOpening` |
| 금고 | 메시와 `OpenOffset`만 정한다. 30초, 3초, 범죄 종류(`VaultWork`)는 C++ 기본값 |

문짝 메시는 콜리전이 있어야 안쪽을 막는다. `ContentsBox`는 크기 0이 기본값이라 보안문과 금고에서는 그대로 둔다.

### 다른 작업과 맞닿는 곳

- **범죄 목격(bizet12):** Interactable의 `CrimeKind`로 연결된다. `ESPCrimeKind` 끝에 `KeycardDoor`, `ContainerOpening`, `ContainerTheft`를 덧붙였다. 기존 값의 순서는 바꾸지 않았다.
- **간접 단서(bizet12):** 열린 보관함과 금고는 `IsClue()`, 드릴 작동은 `IsDrilling()`으로 읽는다. 단서 검사기가 생기면 연결한다(`NOTICE [INDIRECT-CLUE]`).
- **작업 소리:** 소리 범위와 신고 간격이 정해지면 `ReportWorkNoise`로 연결한다(`NOTICE [WORK-NOISE]`).
- **상호작용(공통):** 한 액터에 Interactable이 여럿이면 Interactor가 이 사람이 쓸 수 있는 첫 대상을 고른다. 클라이언트와 서버가 같은 규칙을 쓴다. 다운 확인(`CanOwnerInteract`)은 그대로다.

### 알고 둔 한계

- 거절 안내는 HUD 전까지 화면 디버그 텍스트(`[TEMP-HOLD-DEBUG]`)다. PIE 창들이 디버그 메시지를 함께 쓰므로 클라이언트의 안내도 서버 창에 플레이어 이름과 함께 보인다.
- 문짝은 순간 이동한다. 열림 연출은 나중에 넣는다.

### 확인

- 자동 테스트: `SpacePirate.Openable.Keycard`, `SpacePirate.Openable.Locker`, `SpacePirate.Vault.Direct`, `SpacePirate.Vault.Drill`. 전체 52개 통과.
- PIE: 일부만 확인했다.
- 머지 후 전원 리빌드 필요(`Source/` 변경, 새 클래스 추가).
