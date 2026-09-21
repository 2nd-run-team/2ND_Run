# 2ND_Run

Unreal Engine 5.8 프로젝트.

## 요구 사항

- Unreal Engine **5.8**
- Git + **Git LFS** ([설치](https://git-lfs.com))
- Visual Studio 2022 (C++ 작업 시, "Game development with C++" 워크로드)

## 시작하기

```bash
git lfs install
git clone <repo-url>
cd 2ND_Run
git config lfs.locksverify true
```

`.uproject` 우클릭 → *Generate Visual Studio project files* (C++ 프로젝트인 경우)
→ `.sln` 빌드 → 에디터 실행.

## 폴더 구조

```
Content/      에셋 (LFS 관리, 락 필요)
Config/       프로젝트 설정 — 변경 시 팀 공유 필수
Source/       C++ 소스 (C++ 프로젝트인 경우)
Plugins/      플러그인
```

`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` 는 각자 로컬에서
생성되는 폴더라 저장소에 올리지 않습니다.

## 협업 규칙

에셋 작업 전 **반드시 LFS 락을 먼저 잡으세요.** `.uasset`/`.umap` 은 머지가
불가능해서 동시 수정 시 한쪽 작업이 사라집니다.

자세한 내용은 [CONTRIBUTING.md](CONTRIBUTING.md) 참고.

## 팀

| 이름 | 역할 | GitHub |
|------|------|--------|
|      |      |        |
