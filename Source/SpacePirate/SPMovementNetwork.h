#pragma once

// 역할: 엔진 이동 패킷에 몸 기준 추진 입력과 이동 완료 시점의 몸 회전을 추가한다.
// 구현은 SPCharacterMovementComponent.cpp에 있다(FSavedMove_SP의 정의를 함께 사용).
// NOTICE [ZG-PROTOTYPE]: 현재 입력 직렬화는 디지털 키보드 기준. 아날로그 추진 추가 시 정밀도 재검토.

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementReplication.h"

// 한 번의 이동에 추가로 실어 보낼 데이터.
struct FSPNetworkMoveData : public FCharacterNetworkMoveData
{
    FVector LocalThrustInput = FVector::ZeroVector; // 서버가 같은 추진을 재계산하기 위한 입력.
    FRotator EndBodyRotation = FRotator::ZeroRotator; // 서버의 회전 오차 검사에만 사용. 서버 자세를 덮어쓰지 않는다.

    virtual void ClientFillNetworkMoveData(
        const FSavedMove_Character& ClientMove,
        ENetworkMoveType MoveType) override;

    virtual bool Serialize(
        UCharacterMovementComponent& CharacterMovement,
        FArchive& Ar,
        UPackageMap* PackageMap,
        ENetworkMoveType MoveType) override;
};

// 최신(New), 전송 대기(Pending), 재전송 후보(Old) 기록 모두 같은 확장 데이터 형식을 사용한다.
struct FSPNetworkMoveDataContainer
    : public FCharacterNetworkMoveDataContainer
{
    FSPNetworkMoveDataContainer()
    {
        NewMoveData = &Moves[0];
        PendingMoveData = &Moves[1];
        OldMoveData = &Moves[2];
    }

private:
    FSPNetworkMoveData Moves[3];
};
