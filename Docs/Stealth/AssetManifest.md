# 잠입 시험 맵 애셋 목록

화면에 보이는 문구는 한글 안내로 갱신했다. 수동 플레이 순서와 구역 번호는 [TestGuide_KO.md](TestGuide_KO.md)를 기준으로 한다. 내부 액터 이름과 사건 종류 식별자는 검사·연결 호환을 위해 유지한다.

2026-10-09 KST · UE 5.8.2 · `feature/item-interaction` / `1727f1a` 통합 기반.
외부 애셋 구매/다운로드 없이 엔진 도형과 기존 프로젝트 캐릭터·물건을 사용했다.

## 저장한 애셋

### 원격 작업 통합과 이름 변경

공용 도움말은 `UI/WBP_SPDebugHelp`, 운반 애니메이션은 `Cargo/ABP_SPCargoCarry`와 `Cargo/AS_SPCargoCarryPose`를 사용한다. 옛 이름 3개는 기존 BP 참조를 보존하는 리디렉터다. 다른 팀원이 잠근 플레이어 BP는 수정하지 않았다. 새 이름/호환 파일의 용도와 정리 조건은 [명명 규칙](../AssetNaming.md)을 본다.

기존 `Train/Framework/GameStates/BP_SPTrainGameState`의 부모를 `SPGameState`로 연결했다. 자식 `BP_SPPlanetTrainGameState`의 열차·하늘 기능과 선택된 GameState 클래스는 유지한다. 공통 부모를 바꾸거나 교체할 때 체력·팀 실패/보안 복제와 열차 속도/하늘을 함께 검증해야 한다.

### 한글 안내 보완

기존 표지 18개, 단말 21개, 경비 4명과 레이저 3개의 상태 문구를 한글로 바꿨다. 단말의 E 진행 문구와 복제 HUD·처리 결과도 한글을 사용한다. 키 이름(Ctrl/Space/E/G)과 내부 액터·파일 식별자는 유지한다. 시작 안내판만 안전한 시작 위치 옆으로 옮겼으며 감지 볼륨·게임 규칙·인벤토리는 변경하지 않았다.

| 신규 파일 | 용도 / 교체 방법 |
|---|---|
| `Content/SpacePirate/Stealth/Prototype/UI/F_SPStealthKorean.uasset` | 설치된 맑은 고딕에서 현재 안내에 필요한 글리프를 구운 오프라인 Font. 외부 다운로드·원본 TTF 복사 없음. 새 글자를 추가하면 Font의 Import Options > Chars에 포함한 뒤 콘텐츠 브라우저의 Font 우클릭 > Reimport 후 저장. 숫자·공백·키 이름·가운뎃점도 Chars에 명시해야 함 |
| `Content/SpacePirate/Stealth/Prototype/Materials/M_SPStealthKoreanText.uasset` | FontSample 알파로 마스크, VertexColor로 발광하는 Unlit 재질. TextRender의 Font와 재질의 Font 파라미터를 같은 글꼴로 교체. 여러 페이지 글꼴로 바꾸면 페이지별 표시 검증 필요 |

글꼴·재질을 먼저 LFS로 잠근 뒤 제작했다. 시험 맵과 기존 소유 잠금의 `BP_SPLaserSecurityDevice`, `BP_SPStealthTestConsole`에 표시 참조를 저장했다. 기존 `BP_SPGuardCharacter`도 별도로 잠금·백업한 뒤 기본 TextRender의 글꼴·재질·기본 표시만 갱신했다. 새로 배치하는 경비도 한글 상태를 표시한다. 다른 사람이 잠근 플레이어 BP는 편집하지 않았다. `Tools/Stealth/localize_test_map_ko.py`는 미저장 상태/PIE/잠금을 확인하고 백업 후 저장하며, 이미 생성한 Font가 있으면 덮어쓰지 않는다. 문구만 바꿀 때는 기존 액터의 Text/ButtonLabel/Prompt를 편집하고 새 글리프가 포함됐는지 확인한다.

수동 설명서는 `Docs/Stealth/TestGuide_KO.md`, 이번 제작·빌드·검증 증거는 `Saved/StealthKorean`이다. 아래는 단계 1~5의 제작 이력이며, 현재 화면의 TextRender에는 이 한글 글꼴/재질이 적용된다.

**커밋 전 검토에서 저장 누락 수정:** 앞선 한글화는 메모리의 BP 기본 객체에만 글꼴을 바꿔, 에디터 재시작 시 경비/단말/레이저의 참조가 되돌아갔다. `style_blueprint`를 `Modify` + 에디터 속성 변경 알림 + 강제 패키지 저장 방식으로 고쳤다. 소유 잠금 4개(BP 3종·시험 맵)를 확인하고 백업한 뒤 다시 저장했다. 에디터를 완전히 종료·재실행한 검증에서 BP 기본값과 배치된 TextRender 46개의 참조가 유지됐다. `validate_test_map.py`가 이 4개 검사를 추가로 수행하며 재발을 확인한다. BP 기본값 교체 후 맵만 재로드하는 것은 충분한 저장 검증이 아니다.

경로는 저장소 기준이다. 단계 4까지의 시험 맵·재질 5개·기능 BP 3개를 유지했다. 단계 5에서 레이저 기능 BP 1개, 재질 4개(상태 3·문자 1), 직접 합성한 SoundWave 2개와 원본 WAV 2개를 제작하고 시험 맵에 세 구간을 저장했다. 기존 열차 맵과 기존 BP는 이번 단계에 재저장하지 않았다.

| 파일 | 용도 | 교체·편집 방법 |
|---|---|---|
| `Content/SpacePirate/Maps/Lvl_SPStealthTest.umap` | 107개 액터. 시작점 4개, 경비 4명, 독립 구역 1개, 순찰/NavMesh/엄폐, 단말 21개와 HUD Director, 일반/도달 불가 조사 지점, 실제 포장 2개, 레이저 3개와 안전 공간 | 맵 LFS 잠금 후 UE에서 편집. 바닥/엄폐 변경 후 Build Paths, 저장·재로드·경로 검사 |
| `Content/SpacePirate/Maps/Lvl_SPTrainFreight.umap` | 단계 4에서 기존 경비 구역을 독립 Restricted_Area_Main으로 이관. 경비 2명 발소리 off, 확인 1초. 단계 5에서는 재저장하지 않음 | 구역 Volume만 편집. 기존 열차/행성/GS 부모와 BP 그래프 유지 |
| `Content/SpacePirate/Stealth/Blueprints/BP_SPRestrictedArea.uasset` | 네이티브 SPRestrictedArea 기능 BP. 두 맵의 공통 구역 판정 | Volume의 Box Extent/Transform, AreaName, 활성 여부 편집. 선택적 Visual 참조는 NoCollision. PatrolRoute에 붙이지 않음 |
| `Content/SpacePirate/Stealth/Blueprints/BP_SPStealthTestConsole.uasset` | 기존 E 서버 완료 사건/단계 버튼 및 새 ObservedCrime 시험 행동 | 기존 Command/사건 설정 또는 ObservedCrime + Interactable.CrimeKind/bInstantCrime/HoldDuration/Prompt 지정. 새 범죄 단말은 시야 확정이 필요 |
| `Content/SpacePirate/Stealth/Blueprints/BP_SPStealthTestDirector.uasset` | 네이티브 SPStealthTestDirector 기능 BP. 결과·최초 효과 횟수 복제, 로컬 HUD 생성 | StatusWidgetClass로 UUserWidget 클래스 지정. 현재 기본은 네이티브 SPStealthStatusWidget |
| `Content/SpacePirate/Stealth/Blueprints/BP_SPLaserSecurityDevice.uasset` | 네이티브 SPLaserSecurityDevice 기능 BP. 레벨 제작자가 배치하는 서버 접촉 센서·익명 사건 생산자 | LocalStart/LocalEnd 또는 ReceiverActor, 길이·판정 두께·주기·예고·DeviceId·출동 그룹/범위/인원을 편집. 표현은 별도 메시/상태 재질/사운드 참조 |
| `Content/SpacePirate/Stealth/Prototype/Materials/M_SPStealthNeutral.uasset` | 바닥·외벽 무채색 | Tint/Roughness 파라미터 또는 메시 재질 슬롯 참조 교체 |
| `Content/SpacePirate/Stealth/Prototype/Materials/M_SPStealthPublic.uasset` | 일반 공간 초록 표시 | 위와 동일. 색상은 판정에 사용하지 않음 |
| `Content/SpacePirate/Stealth/Prototype/Materials/M_SPStealthRestricted.uasset` | 제한 공간 붉은 표시 | 실제 제한 박스는 Restricted_Area_Main.Volume에서 별도 편집 |
| `Content/SpacePirate/Stealth/Prototype/Materials/M_SPStealthCover.uasset` | 엄폐·테이블·시험 단말의 표현 | 재질/메시 교체. 엄폐의 Visibility/Pawn 충돌은 별도 확인 |
| `Content/SpacePirate/Stealth/Prototype/Materials/M_SPStealthFuture.uasset` | 확장 구역 바닥·표지 | 색상/바닥은 장치·목표 판정을 갖지 않음 |
| `Content/SpacePirate/Stealth/Prototype/Materials/M_SPLaserOn.uasset` | 레이저 켜짐의 붉은 발광, Unlit | Tint 벡터 파라미터 또는 장치 OnMaterial 참조 교체. 색은 판정에 사용하지 않음 |
| `Content/SpacePirate/Stealth/Prototype/Materials/M_SPLaserOff.uasset` | 꺼짐의 어두운 파랑, Unlit | Tint 또는 OffMaterial 참조 교체. OFF / SAFE 상태 문자를 함께 표시 |
| `Content/SpacePirate/Stealth/Prototype/Materials/M_SPLaserWarning.uasset` | 켜짐/꺼짐 전환 예고의 노랑·주황 발광, Unlit | Tint 또는 WarningMaterial 참조 교체. ON→OFF는 아직 위험, OFF→ON은 아직 안전 |
| `Content/SpacePirate/Stealth/Prototype/Materials/M_SPLaserText.uasset` | 상태 문자와 레이저 구간 표지의 조명 영향 없는 표시. 엔진 DefaultTextMaterialOpaque에서 파생 | TextRender TextMaterial 참조 교체. 폰트 텍스처/마스크 연결과 VertexColor→Emissive 연결을 유지해 글자 형태·상태색을 보존 |
| `Content/SpacePirate/Stealth/Prototype/Audio/S_SPLaserWarning.uasset` | 직접 합성한 전환 예고음. 700→1000Hz, 0.16초 | 원본 WAV 편집 후 SoundWave Reimport 또는 WarningSound 참조 교체 |
| `Content/SpacePirate/Stealth/Prototype/Audio/S_SPLaserContact.uasset` | 새 Applied 레이저 사건 신호음. 900→420Hz, 0.24초 | 원본 WAV 편집 후 SoundWave Reimport 또는 ContactSound 참조 교체. Duplicate마다 재생하지 않음 |
| `Content/SpacePirate/Stealth/Prototype/Audio/Source/S_SPLaserWarning.wav` | 예고음 원본. PCM16·모노·22050Hz·0.16초 | WAV도 LFS lockable. 합성식은 setup_laser_test_map.py에 보존 |
| `Content/SpacePirate/Stealth/Prototype/Audio/Source/S_SPLaserContact.wav` | 접촉음 원본. PCM16·모노·22050Hz·0.24초 | WAV 편집/재가져오기 가능. 생성기는 기존 원본 덮어쓰기 거부 |

모든 바이너리는 생성/편집 전에 LFS 잠금을 확보했다. 단계 5는 신규 9개 파일(BP 1·재질 4·SoundWave 2·WAV 2)의 소유 잠금을 먼저 확보하고 기존 시험 맵 잠금 55026556을 사용했다. 제작 스크립트도 `locks --verify --json`의 ours와 대상 경로를 대조한 뒤 실행한다. 문자 재질은 실제 화면에서 기존 문자가 어두운 것을 확인한 뒤 별도 잠금을 확보해 추가했다. 기존 캐릭터·열차/경비 BP·애니메이션·메시·재질 5개·단말/Director/구역 BP 및 열차 맵은 이번 단계에 재저장하지 않았다. 미커밋/미푸시 상태이므로 잠금을 유지한다. 공유/푸시 후 CONTRIBUTING 규칙에 따라 해제하며 남의 잠금을 강제로 해제하지 않는다.

이전 단계 이력: 단계 4에서 맵은 82개 액터/경비 3명이었고 구역 BP와 시험/열차 맵을 저장했다. 당시 LFS 서버 일시 오류 동안 저장을 보류하고 복구 후 ours를 확인했다. 열차 맵 55047838, 구역 BP 55047839는 그 단계의 잠금이다.

## 레이저 구간 배치

`StealthTest/80_LaserSecurity`에 세 레이저, 안전/출구 패드·문턱·표지·발신기/수신기 지지대, 전용 경비를 배치했다. 전체 25개 액터 추가이며 맵 자체에 저장된다. 별도 메시 에셋을 생성하지 않고 엔진 Cube/Sphere와 기존 재질을 재사용했다.

| 배치 | 센서 원점(cm) / 빔 | 안전 공간 중심(cm) |
|---|---|---|
| `Laser_Crouch` | (-2350,1800,0), +X 500cm, 높이 145cm, 상시 켜짐 | (-2100,1450,2) |
| `Laser_Jump` | (-250,1800,0), +X 500cm, 높이 40cm, 상시 켜짐 | (0,1450,2) |
| `Laser_Wait` | (1850,1800,0), +X 500cm, 높이 95cm, 켜짐/꺼짐 각 4초·예고 1초 | (2100,1450,2) |

세 장치 모두 판정 전체 두께 4cm, 출동 그룹 StealthTest_Laser, 반경 5000cm·최대 1명·호출 간격 2초다. 바닥 센서 원점을 사건 위치로 사용하며 빔 접촉자 위치를 전달하지 않는다. 안전 패드는 650×380cm, 출구 패드는 각 중심 X의 Y=2040에 650×230cm다. `Laser_ResponseGuard`는 (-800,1450,100)에 배치한 기존 경비 BP로 같은 출동 그룹, 신원 공유 범위 StageSecurity, 발소리 off를 사용한다. 세 구간은 제한 구역 밖이며 도달 가능한 NavMesh 위에 있다.

실제 플레이어의 반경 34cm·반높이 96cm, 앉기 반높이 56cm, JumpZ 420cm/s로 호스트/참가자 양쪽의 Ctrl/Space 통과를 확인했다. 실행 방법, 주기와 보수적인 같은 프레임 자세 변경 판정, 측정 높이·경로는 [LaserSecurity.md](LaserSecurity.md)에 기록했다.

## 표현과 판정의 분리

- 시험 단말의 **InteractionVolume**은 Visibility만 차단하는 Query 박스다. **Visual**은 NoCollision이며 StaticMesh/Material 참조를 편집할 수 있다. 큐브 대신 다른 메시를 쓰거나 제거해도 사건 종류·상호작용 범위는 바뀌지 않는다.
- ButtonLabel과 TextRenderComponent의 글꼴/색/크기/위치를 편집한다. 단말 위치와 사건 위치 IncidentLocation은 별도다. 기존 단말은 경비 구역, 새 두 단말은 일반 구역 (-650,600,0) / 공중 (-650,600,900)으로 전달한다. 새 단말은 반경 5000cm·최대 1명이다.
- `Public_VisualFloor`, `Restricted_VisualFloor`, `Restricted_Threshold`, 확장 공간의 바닥색은 NoCollision이며 게임 규칙이 없다.
- 제한 구역은 `Restricted_Area_Main.Volume`의 실제 변환/Box Extent를 사용한다. 기존 경로 박스는 직렬화 호환용이며 감지에 쓰지 않는다. 시각 패널과 자동 동기화하지 않으므로 위치를 바꾸면 둘을 확인한다.
- 엄폐/바닥 Cube는 BlockAll. 메시 실루엣이 아니라 실제 Collision/Visibility 응답이 이동·시야를 막는다. 교체 후 NavMesh와 `CanSeePlayer`를 검사한다.
- `debug_test_map.py`는 주황 제한 박스, 청록 청각 범위, 시야 캡슐 샘플, 노란 익명 수색 목적지, 보라 단말 판정 박스를 표시한다.
- `Investigation_Point_Public/Unreachable`은 편집용 TargetPoint, `Investigation_Visual_*`은 엔진 Cylinder와 기존 재질, `Investigation_Sign_*`은 TextRender다. 표현은 **NoCollision 프로필**이며 공중 표시는 실제 발판이 아니다. 지점과 단말 IncidentLocation은 별도이므로 이동 시 둘을 갱신한다.
- `Crime_PublicNormal_Floor/Sign`, `Crime_PublicWork_Floor/Sign`은 기존 재질과 엔진 Cube/TextRender로 제작한 충돌 없는 표식이다. `Crime_NoiseWall`은 실제 Visibility/Pawn 차단, `Crime_PackingTable`은 BlockAll 테이블이다. 메시 교체 시 해당 충돌과 NavMesh를 검사한다.
- `Crime_Pickpocket/Terminal/Vault/Packing/Tool`은 기존 기능 단말 BP의 인스턴스다. Cube Visual/재질/문자를 편집해도 범죄 종류·시간은 Interactable 설정을 사용한다. `Crime_WorkNoise`는 CrimeKind=None인 익명 작업 소리 버튼이다.
- `Crime_PublicLootBundle`은 기존 포장 BP 참조이며 실제 완료 시 기존 LootBagClass 하나를 생성한다. 메시/포장 결과 참조는 기존 컴포넌트에서 교체한다. `Crime_ObserverGuard`는 기존 경비 BP이며 Observer의 상대 위치/회전을 눈으로 편집한다.
- 레이저 `DetectionVolume`은 빔 방향의 독립 QueryOnly 박스다. 서버가 실제 플레이어 캡슐의 현재 접촉과 켜진 시간 구간의 sweep을 검사한다. LocalStart/LocalEnd/ReceiverActor·BeamLength·DetectionThickness가 판정을 정하며 메시 bounds·이름·애니메이션 길이는 사용하지 않는다. 편집기 끝점/박스와 PIE의 bDrawDetection으로 실제 범위를 확인한다.
- 레이저 `EmitterVisual/ReceiverVisual/BeamVisual`은 NoCollision, NavMesh 영향 없음이다. 발신기 Cube·수신기 Sphere·빔 Cube를 교체하거나 제거해도 판정은 유지된다. 빔 교체 메시의 중앙 피벗과 길이축 X를 맞추고 VisualThickness를 편집한다. 메시 bounds는 **표현 크기를 맞출 때만** 사용한다. 세 상태 재질 참조가 각 메시 슬롯 0에 적용되므로 단일 슬롯 덮어쓰기 대신 OnMaterial/OffMaterial/WarningMaterial을 교체한다.
- 레이저 상태 문자 및 구간 표지 6개에 `M_SPLaserText`를 연결해 주변 조명과 무관하게 읽히도록 했다. 이 재질은 엔진 폰트 재질의 글리프 텍스처/마스크를 유지하고 VertexColor를 Emissive에 연결한다. 교체 시 TextRender의 TextMaterial/Font를 지정하고 글자 마스크와 컴포넌트 색상 입력을 유지한다. 일반 빔 발광 재질을 그대로 문자에 넣어 마스크를 잃지 않도록 한다.
- 레이저 원점은 경비가 접근할 바닥 위치, 빔 높이는 LocalStart/LocalEnd.Z로 편집한다. 안전/출구 패드·문턱·지지대는 NoCollision 표시다. 이동 가능한 ReceiverActor를 사용하면 해당 액터 위치 복제를 별도로 연결한다. 런타임 서버 설정 변경 뒤 RefreshDevice, 새 주기/계수 초기화는 ResetDevice를 사용한다.
- 레이저 WarningSound/ContactSound는 편집 가능한 SoundBase 참조다. WAV와 SoundWave를 교체해도 재생 길이는 주기·호출 간격·사건 완료에 영향을 주지 않는다. 스크립트가 직접 합성한 원본을 제공하며 실제 주관적 청음/믹싱 평가는 별도다.
- 경비 AlertIndicator는 복제 GuardState를 읽어 PATROL/CHECK SIGHT/INVESTIGATE/SEARCH SCENE/CHASE/LAST SEEN / MOVE/SEARCH LAST SEEN/LISTEN을 표시한다. 맵 인스턴스 크기는 24. bShowStateIndicator로 끄거나 TextRender 글꼴/재질을 교체할 수 있다. 게임 판정은 문자와 무관하다.
- 임시 HUD는 네이티브 UMG다. 이번 단계에도 WBP/메시/애니메이션 파일은 추가하지 않았고 신호음 파일은 새로 추가했다. 표시 WBP를 제작하면 `Content/SpacePirate/Stealth/Prototype/UI`에 두고 Director.StatusWidgetClass 참조를 바꾼다. 새 위젯은 복제 GS/PS의 읽기 API와 OnStateChanged를 사용한다.
- 단말/HUD는 Development 시험용이며 Shipping/Test에서 실행하지 않는다. 레이저 장치는 별도 네이티브 런타임 액터다. 임시 범죄 단말은 실제 민간인·제압·금고·탈출 게임플레이의 대체물이 아니다.
- 전체 경보/발소리 오디오는 아직 없다. 현재 실제 연결한 오디오는 레이저 예고/접촉 신호음 2개다. 후속 사운드는 편집 가능한 참조와 서버 최초 이벤트에 연결하며 재생 길이를 게임 완료 조건으로 쓰지 않는다.
- 엔진 기본 DirectionalLight/SkyLight/SkyAtmosphere 및 맵 전용 노출을 사용한다. 열차 배경·행성·환경 프리셋 의존성은 없다.

## 재사용 참조

| 참조 | 역할 / 교체 지점 |
|---|---|
| `/Engine/BasicShapes/Cylinder` | 일반/공중 조사 좌표의 충돌 없는 표시. 교체해도 좌표·반경은 단말 설정을 사용 |
| `/Engine/BasicShapes/Cube` | 바닥, 벽, 엄폐, 테이블, 단말, 레이저 발신기·빔·구간 표식·지지대 표현. 각 StaticMeshComponent에서 교체 |
| `/Engine/BasicShapes/Sphere` | 레이저 수신기 표현. ReceiverVisual에서 교체 |
| `/Game/SpacePirate/Core/Blueprints/BP_SPGameMode` | World Settings GameMode Override. 기존 팀 Pawn/Controller 유지. 네이티브 부모가 기존 GS/PS에 보안 컴포넌트를 연결 |
| `/Game/SpacePirate/Player/Blueprints/BP_SPPlayerCharacter` | 기존 플레이어/중력/이동/인벤토리/E. Mesh/Anim Class/Crouch Pose Class/Input은 기존 편집 가능 참조 |
| `/Game/SpacePirate/Player/Blueprints/BP_SPPlayerController` | 기존 입력/카메라. 시험 맵에 열차 컨트롤러 불필요 |
| `/Game/SpacePirate/Stealth/Blueprints/BP_SPGuardCharacter` | PatrolRoute, AlertGroup(출동), IdentityScope(신원 공유), 감지/이동 값을 각각 편집 |
| `/Game/SpacePirate/Stealth/Animation/ABP_SPGuardLocomotion` | 기존 경비 걷기/달리기. Mesh Anim Class에서 교체 |
| `/Game/SpacePirate/Stealth/Animation/ABP_SPCrouchPostProcess` | 기존 Manny 앉기 후처리. Crouch Pose Class에서 교체 |
| 기존 Manny, `ABP_SPCargoCarry`, `BS_Idle_Walk_Run` | 외형/운반/이동 자세. 운반 BP/시퀀스만 SP 규칙에 맞춰 이름 변경. 원본 재제작 없음 |
| `/Game/SpacePirate/Cargo/BP_SPCargo` | Test_Keycard 기존 시험 화물 |
| `/Game/SpacePirate/Cargo/BP_SPLootBag` | Test_LootBag 및 포장 결과 |
| `/Game/SpacePirate/Cargo/BP_SPLootBundle` | 기존 E 2초 포장. LootBagClass로 결과 가방 지정 |
| `/Game/SpacePirate/UI/WBP_SPHoldProgress` | 기존 진행률 표시. Interactor.HoldProgressWidgetClass로 교체 |

## 제작·검증 소스

| 파일 | 용도와 재실행 주의 |
|---|---|
| `Tools/Stealth/create_test_map.py` | 단계 1 신규 맵 생성. 기존 맵 덮어쓰기 거부 |
| `Tools/Stealth/setup_incident_test_map.py` | 단계 1 맵에 BP/단말/Director 추가. 잠금·dirty 검사, Saved 백업, 실제 저장. 이미 구성되면 실행 거부 |
| `Tools/Stealth/setup_investigation_test_map.py` | 단계 2 맵에 지점/표현/단말 8개 추가. 기존 구성 덮어쓰기 거부, 맵 잠금 확인·백업·저장 |
| `Tools/Stealth/setup_crime_test_map.py` | 단계 3 맵과 기존 열차 맵에 독립 구역 이관, 범죄 시험 공간 추가. 3개 LFS 잠금·dirty 검사·백업 후 BP/맵 저장, 이미 생성된 BP 덮어쓰기 거부 |
| `Tools/Stealth/setup_laser_test_map.py` | 단계 4 저장 맵에 레이저 세 구간 추가. 시험 맵+신규 9파일의 LFS 잠금·dirty 확인·Saved 백업 후 BP/상태·문자 재질/합성 WAV/SoundWave와 맵 저장. 이미 만든 BP/원본 덮어쓰기 거부. NavMesh 완료 후 추가 저장 필요 |
| `Tools/Stealth/validate_laser_map.py` | 저장·재로드한 맵에서 세 센서 설정/안전 공간/완전 NavMesh 경로/표현 분리/오디오 참조와 PCM 원본 등 25개 검사. Saved/StealthLaser/map-validation.json |
| `Tools/Stealth/verify_laser_pie.py` | 새 2인 Listen PIE에서 양쪽 실제 Ctrl/Space/이동·E 입력, 익명 조사·접촉 구분·주기·복제·10FPS 빠른 이동·메시 교체·단계 초기화 39개 검사. Saved/StealthLaser/laser-pie.json |
| `Tools/Stealth/verify_crime_pie.py` | 실제 E/이동 입력·시야·범죄·구역 UI·복제 26개 검사. 결과 Saved/StealthCrime/crime-pie.json |
| `Tools/Stealth/verify_investigation_pie.py` | 새 2인 PIE에서 실제 조사/수색/추격/실패·복제·새 E 단말 31개 검사 |
| `Tools/Stealth/validate_test_map.py` | 저장 맵 경로·참조·충돌·단말·재귀 의존성 읽기 전용 검사 |
| `Tools/Stealth/verify_test_map_pie.py` | 새 2인 PIE에서 기존 경비/이동/인벤토리 E/G 회귀 23개 |
| `Tools/Stealth/verify_incidents_pie.py` | 새 2인 PIE에서 사건/복제/UI/권한/단계 검사 27개 |
| `Tools/Stealth/verify_incident_buttons_pie.py` | 저장된 단말의 실제 참가자·호스트 E 입력 검사 19개 |
| `Tools/Stealth/verify_train_security_pie.py` | 기존 열차 맵 GS 속도/하늘/보안 초기화 9개 + 독립 구역/경비 설정 2개 |
| `Tools/Stealth/debug_test_map.py` | 실제 판정 범위/익명 수색 위치 디버그 토글 |
| `Source/SpacePirate/SPStealthTypes.h`, `SPStealth*Component.*` | 사건 정의 및 PS/GS 복제 원본 |
| `Source/SpacePirate/SPStealthTestConsole.*`, `SPStealthStatusWidget.*` | 시험 단말/Director/임시 UMG |
| `Source/SpacePirate/Tests/SPStealthIncidentTests.cpp` | 사건 정책·최초 효과·중복·수명·검증 규칙 자동 테스트 |
| `Source/SpacePirate/SPRestrictedArea.*`, `SPStealthObserverComponent.*`, `SPStealthActivityComponent.*` | 독립 구역, 공통 목격 증거, 서버 작업 등록/정리와 구역 복제 |
| `Source/SpacePirate/Tests/SPStealthCrimeTests.cpp` | 범죄 자동 테스트 8개. 초기화 콜백 재진입과 중력 방향 시야 회귀 포함 |
| `Source/SpacePirate/SPLaserSecurityDevice.h`, `SPLaserSecurityDevice.cpp` | 편집 가능한 레이저 액터, 복제 주기/표현, 서버 캡슐 접촉·sweep·호출 간격·익명 사건 |
| `Source/SpacePirate/Tests/SPLaserSecurityTests.cpp` | 레이저 자동 테스트 6개. 주기·끝점/길이, 익명 정책·별도 목격, 접촉 구분/간격, 0.2cm 빔·꺼짐/긴 프레임, 캡슐/회전·외형 독립, 단계/센서 이동 수명 |
| `Docs/Stealth/ImplementationStatus.md`, `IncidentAPI.md`, `GuardInvestigation.md`, `CrimeObservation.md`, `LaserSecurity.md` | 실행 상태·결과·한계, 서버 연결 계약, 장치 제작·교체 방법 |

제작 스크립트가 중간에 실패하면 자동 롤백하지 않는다. 부분 결과와 Saved 백업을 확인한다. 기존 맵을 삭제해 생성기를 다시 실행하면 수동 편집을 잃으므로 사용하지 않는다.

단계 5에서 최종 Editor 빌드 성공, 자동 테스트 39/39(신규 레이저 6개 포함), 신규 레이저 PIE 39/39, 기존 시험 맵 PIE 회귀 126/126, 저장 맵 검사 신규 25/25·기존 32/32를 실제 수행했다. 신규 결과는 `Saved/StealthLaser/build-final.log`, `AutomationFinal/index.json`, `laser-pie.json`, `regression.json`, `map-validation.json`에 있다. 이전 단계 성공을 새 실행 결과로 대신 기록하지 않았다.

전체 증거는 `Saved/StealthLaser`, `Saved/StealthCrime`, `Saved/StealthInvestigation`, `Saved/StealthTest`, `Saved/StealthEvents`, `Saved/Stealth`, `Saved/Logs`에 저장되며 런타임/배포 의존성이 아니다. 개인 연결 도구 없이도 에디터 Cmd의 `py "<프로젝트>/Tools/Stealth/<스크립트>.py"`로 실행할 수 있다. 실행 결과·미실행 항목은 [ImplementationStatus.md](ImplementationStatus.md)를 기준으로 한다.
