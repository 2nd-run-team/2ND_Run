// 작성자 : 임진혁
#include "Prototype01/SP1GravityCargo.h"
#include "SPGravityZone.h"
#include "Components/StaticMeshComponent.h"
ASP1GravityCargo::ASP1GravityCargo()
{
    Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
    Core->SetupAttachment(RootComponent);
    Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Core->SetSimulatePhysics(false);
    // 보유 팩에 독립 링 메시가 없어 여덟 조각의 고정 프레임으로 특수 화물을 구분한다.
    for (int32 Index=0; Index<8; ++Index)
    {
        auto* Segment = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Ring%d"),Index));
        Segment->SetupAttachment(RootComponent);
        Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        const float Angle = Index*PI/4;
        Segment->SetRelativeLocation(FVector(0,FMath::Sin(Angle)*45,100+FMath::Cos(Angle)*45));
        Segment->SetRelativeRotation(FRotator(0,0,-Index*45));
        Segment->SetRelativeScale3D(FVector(.08f,.37f,.08f));
        RingSegments.Add(Segment);
    }
}
void ASP1GravityCargo::SetTransferred(bool bNewTransferred)
{
    if (!HasAuthority()) return;
    if (GravityZone) GravityZone->SetGravityMode(bNewTransferred ? ESPGravityMode::ZeroGravity : ESPGravityMode::Gravity);
    Super::SetTransferred(bNewTransferred);
}
void ASP1GravityCargo::OnRep_Transferred()
{
    Super::OnRep_Transferred();
    Core->SetVisibility(!bTransferred);
    for (const auto& Segment : RingSegments) Segment->SetVisibility(!bTransferred);
}
