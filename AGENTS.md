# 에이전트 작업 지침

Codex · Claude Code 등 코딩 에이전트가 이 저장소에서 작업할 때 따르는 규칙입니다.
사람용 문서는 [README.md](README.md) · [CONTRIBUTING.md](CONTRIBUTING.md) 를 보세요.

---

## 1. 프로젝트 사실

| 항목 | 값 |
|------|-----|
| 엔진 | Unreal Engine **5.8.2** (`D:\UE_5.8`) — 버전 고정, 임의 변경 금지 |
| 프로젝트 파일 | `SecondRun.uproject` (저장소명 `2ND_Run` 과 다름) |
| C++ 모듈 | `SecondRun` (`Source/SecondRun/`) |
| 기본 레벨 | `Content/FirstPerson/Lvl_FirstPerson.umap` |
| 에셋 관리 | Git LFS + 파일 락 (`.uasset` / `.umap` 은 `lockable`) |

코어 C++ 클래스 — UE FirstPerson 템플릿 기반:

```
ASecondRunCharacter        Source/SecondRun/SecondRunCharacter.h
ASecondRunGameMode         Source/SecondRun/SecondRunGameMode.h
ASecondRunPlayerController Source/SecondRun/SecondRunPlayerController.h
ASecondRunCameraManager    Source/SecondRun/SecondRunCameraManager.h
```

`Source/SecondRun/Variant_Horror/` · `Variant_Shooter/` 는 템플릿에 딸려온
예제입니다. 코어가 참조하지 않으므로 **참고용으로만 읽고, 새 기능을 여기에
추가하지 마세요.** 정리 예정입니다.

---

## 2. 에이전트가 할 수 없는 일

이걸 먼저 읽으세요. 대부분의 사고가 여기서 납니다.

### 블루프린트와 에셋은 편집할 수 없습니다

`.uasset` · `.umap` 은 **바이너리**입니다. 텍스트 도구로 열거나 수정할 수
없고, 시도하면 파일이 깨집니다. 되돌릴 방법이 없습니다.

블루프린트 그래프를 "읽었다" 고 말하지 마세요. 읽을 수 없습니다. 에셋의
내용이 필요하면 **사람에게 에디터에서 확인해 달라고 요청하세요.**

에셋 변경이 필요한 작업은 다음 중 하나로 처리합니다:

1. C++ 쪽에 로직을 만들고, 블루프린트가 그걸 상속·호출하게 한다 (권장)
2. 사람이 에디터에서 할 작업을 **단계별로 적어서 넘긴다**

### 에디터를 실행할 수 없습니다

PIE(Play In Editor) 테스트, 에셋 확인, 레벨 배치는 전부 사람이 합니다.
"동작을 확인했다" 는 말은 **빌드가 통과했다** 는 뜻 이상으로 쓰지 마세요.

### 커밋된 것을 되돌리는 비용이 큽니다

에셋은 머지가 불가능하고, 히스토리 재작성은 팀 전원의 클론을 깨뜨립니다.
`git push --force`, `git rebase`, `git reset --hard` 를 공용 브랜치에
쓰지 마세요.

---

## 3. 빌드

작업 후 **반드시 빌드해서 통과를 확인**하고, 그 결과를 보고하세요.

```bash
# 1) 프로젝트 파일 생성 (새 UCLASS 추가·파일 추가/삭제 후 필요)
"D:/UE_5.8/Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe" \
  "D:/UE_5.8/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" \
  -projectfiles -project="D:/Unreal Projects/2ND_Run/SecondRun.uproject" -game -engine -progress

# 2) 에디터 타깃 빌드
"D:/UE_5.8/Engine/Build/BatchFiles/Build.bat" SecondRunEditor Win64 Development \
  -Project="D:/Unreal Projects/2ND_Run/SecondRun.uproject" -WaitMutex
```

`Result: Succeeded` 가 나와야 통과입니다.

> **주의:** `UnrealBuildTool.exe` 를 직접 실행하면 .NET 10 런타임을 못 찾아
> 실패합니다. 위처럼 엔진 번들 `dotnet.exe` 로 `.dll` 을 실행하세요.

빌드가 계속 깨지면 `Intermediate/` · `Binaries/` 를 지우고 1번부터 다시 합니다.

### 에디터가 켜져 있으면 실패합니다

`UnrealEditor.exe` 가 실행 중이면 파일 잠금으로 빌드·파일 조작이 막힙니다
(`Permission denied`, `Device or resource busy`). **프로세스를 임의로 종료하지
말고** 사람에게 에디터를 닫아 달라고 요청하세요. 저장 안 된 작업이 날아갑니다.

---

## 4. C++ 규칙

### 언리얼 관례를 따릅니다

- 접두사: `A` 액터, `U` UObject, `F` 구조체, `E` 열거형, `I` 인터페이스
- 리플렉션이 필요한 멤버에 `UPROPERTY()`, 함수에 `UFUNCTION()`
- 객체 참조는 `TObjectPtr<T>`, 컨테이너는 `TArray` · `TMap` · `TSet`
- 문자열은 `FString` · `FName` · `FText` (용도 구분). `std::string` 금지
- 로그는 `UE_LOG`. `printf` · `std::cout` 금지

### 블루프린트가 상속한 클래스는 함부로 못 바꿉니다

**C++ 클래스명·프로퍼티명 변경과 삭제는 블루프린트를 깨뜨립니다.** 에러 없이
조용히 깨지고, 복구하려면 에셋을 다시 만들어야 합니다.

불가피하면:
1. 먼저 사람에게 알리고 합의
2. `Config/DefaultEngine.ini` 에 `[CoreRedirects]` 추가
3. PR 본문에 명시

`ASecondRunCharacter` · `ASecondRunGameMode` · `ASecondRunPlayerController` ·
`ASecondRunCameraManager` 는 **전부 블루프린트가 상속 중입니다.** 이름 변경
대상이 아닙니다.

### 의존성 추가는 눈에 띄게

`.Build.cs` 의 `PublicDependencyModuleNames` 에 모듈을 추가했으면 PR 본문에
쓰세요. 팀 빌드가 깨지는 원인 1순위입니다.

### 프로젝트 공통 규칙

- 파일은 작게. 한 파일 800줄을 넘기지 않습니다
- 함수는 50줄 이내
- 하드코딩된 값 대신 `UPROPERTY(EditAnywhere)` 로 빼서 디자이너가 조정하게 합니다
- 에러를 조용히 삼키지 않습니다. `check()` (치명적) 과 `ensure()` (복구 가능) 를 구분해 씁니다

---

## 5. Git

### 브랜치

`develop` 에서 따고, 이름은 **`<타입>/<작업>`** 입니다. 작성자 이름은 넣지 않습니다.

| 타입 | 담당 | 예 |
|------|------|-----|
| `feature/` | 프로그래머 — 코드·기능·버그 수정·설정·문서 | `feature/player-dash` |
| `art/` | 아티스트 — 에셋·머티리얼·레벨 | `art/character-material` |
| `sound/` | 사운드 — BGM·효과음 | `sound/footstep-sfx` |

`main` · `develop` 직접 푸시 금지. 반드시 PR 을 거칩니다.

### 커밋

```
<type>: <요약>

<본문 — 왜 바꿨는지>
```

타입: `feat` `fix` `refactor` `art` `level` `docs` `chore` `ci`

**"무엇을" 이 아니라 "왜" 를 씁니다.** diff 를 보면 무엇인지는 알 수 있습니다.

### 절대 커밋하지 않는 것

```
Binaries/  Intermediate/  Saved/  DerivedDataCache/  *.sln  *.slnx  .vs/  .vsconfig
```

`.gitignore` 가 막고 있습니다. `git add -f` 로 우회하지 마세요.

### 에셋을 건드려야 한다면 락

```bash
git lfs lock Content/경로/파일.uasset     # 작업 전
git lfs locks                              # 현황 확인
git lfs unlock Content/경로/파일.uasset   # 푸시 후
```

`.uasset` · `.umap` 은 머지가 불가능해서, 락 없이 동시 수정하면 한쪽 작업이
통째로 사라집니다. `Source/` 의 `.cpp` · `.h` 는 락이 필요 없습니다.

---

## 6. 작업 보고

### 사실만 씁니다

- 빌드가 통과하면 "빌드 통과 (Result: Succeeded)" 라고 씁니다
- 빌드가 실패하면 **실패했다고 쓰고 에러 원문을 붙입니다.** 숨기지 않습니다
- 실행해보지 않았으면 "빌드만 확인, PIE 미검증" 이라고 명시합니다
- 건너뛴 작업이 있으면 무엇을 왜 건너뛰었는지 씁니다

### 사람이 해야 할 일은 따로 적습니다

에이전트가 못 하는 작업(에셋 편집, 레벨 배치, PIE 테스트)이 남으면
**체크리스트로 만들어 넘기세요.** "나머지는 에디터에서 하시면 됩니다" 는
부족합니다. 어느 에셋을 열어 무엇을 바꿔야 하는지 적습니다.

### PR

`.github/pull_request_template.md` 의 체크리스트를 실제로 검증하고 채웁니다.
확인 안 한 항목에 체크하지 마세요.

`Source/` 를 건드렸으면 **"머지 후 전원 리빌드 필요"** 를 PR 본문에 씁니다.

---

## 7. 막혔을 때

추측해서 진행하지 말고 물어보세요. 특히:

- 에셋 내부 구조를 알아야 진행되는 경우
- 기존 블루프린트 로직과 충돌할 가능성이 있는 경우
- 클래스 이름·구조를 바꿔야 하는 경우
- `Config/*.ini` 를 크게 손봐야 하는 경우 (전원의 에디터에 영향)

되돌릴 수 없는 작업 앞에서는 멈추고 확인받는 편이 항상 쌉니다.
