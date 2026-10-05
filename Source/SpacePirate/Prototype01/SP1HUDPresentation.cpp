// 작성자 : 임진혁
#include "Prototype01/SP1HUDWidget.h"
#include "Prototype01/SP1RoundComponent.h"
#include "Prototype01/SP1SessionSubsystem.h"
#include "Prototype01/SP1InteractionComponent.h"
#include "Prototype01/SP1TransferCargo.h"
#include "Prototype01/SP1GravityCargo.h"
#include "Prototype01/SP1CargoDefinition.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/AudioComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void USP1HUDWidget::StopAudio()
{
    if (IsValid(FeedbackAudio)) FeedbackAudio->Stop();
    if (IsValid(AlertAudio)) AlertAudio->Stop();
    FeedbackAudio=nullptr; AlertAudio=nullptr;
}
void USP1HUDWidget::PlayFeedback(USoundBase* Sound,float Volume)
{
    if (IsValid(FeedbackAudio)) FeedbackAudio->Stop();
    FeedbackAudio=Sound && USP1SessionSubsystem::CanPlayAudio(this) ? UGameplayStatics::SpawnSound2D(this,Sound,Volume) : nullptr;
    USP1SessionSubsystem::TrackAudio(this,FeedbackAudio);
}
void USP1HUDWidget::PlayCue(FName Cue,bool bAlert)
{
    // 사건 관측과 실제 재생을 분리한다. 음소거 중에도 화면 경고/재생 이력의 의미는 유지한다.
    AudioEvents.Add(Cue); if (AudioEvents.Num()>64) AudioEvents.RemoveAt(0);
    const auto* Found=Cues.Find(Cue);
    USoundBase* Sound=Found ? Found->Get() : nullptr;
    if (!bAlert) { PlayFeedback(Sound,.3f); return; }
    if (IsValid(AlertAudio)) AlertAudio->Stop();
    AlertAudio=Sound && USP1SessionSubsystem::CanPlayAudio(this) ? UGameplayStatics::SpawnSound2D(this,Sound,.45f) : nullptr;
    USP1SessionSubsystem::TrackAudio(this,AlertAudio);
}
void USP1HUDWidget::UpdateAudio(const FSP1RunState& Run,double Time)
{
    auto* Session=GetGameInstance()->GetSubsystem<USP1SessionSubsystem>();
    if (ShownRunId!=Run.RunId)
    {
        StopAudio(); ShownRunId=Run.RunId; AudioEvents.Reset(); bWasHeld=false;
        bWarning60=false; bWarning30=false; LastCountdown=-1; LastDepartureDeadline=0; LastPhase=-1;
        LastReason=NAME_None; ReasonVisibleUntil=0;
    }
    const bool bPlaying=SP1::IsPlaying(Run.Phase);
    const double Remaining=bPlaying ? SP1::MissionRemaining(Run,Time) : Run.MissionLimit;
    auto Announce=[&](FName Event,FName Cue)
    { if (Session && Session->ClaimAnnouncement(Run.RunId,Event)) PlayCue(Cue,true); };
    if (SP1::IsTerminal(Run.Phase))
    {
        if (LastPhase!=static_cast<int32>(Run.Phase))
        { StopAudio(); Announce(TEXT("Result"),Run.Phase==ESP1Phase::Succeeded ? TEXT("Success") : TEXT("Failure")); }
        bWasHeld=false; return;
    }
    if (bPlaying && Remaining<=30 && !bWarning30)
    { bWarning30=true; bWarning60=true; Announce(TEXT("Time30"),TEXT("Time30")); }
    else if (bPlaying && Remaining<=60 && !bWarning60)
    { bWarning60=true; Announce(TEXT("Time60"),TEXT("Time60")); }
    if (Run.Phase==ESP1Phase::ExtractionCountdown)
    {
        if (LastDepartureDeadline!=Run.ExtractionDeadline)
        { LastDepartureDeadline=Run.ExtractionDeadline; Announce(TEXT("Departure"),TEXT("Departure")); }
        const int32 Count=FMath::CeilToInt(Run.ExtractionDeadline-Time);
        if (Count>=1 && Count<=5 && Count!=LastCountdown)
        {
            LastCountdown=Count;
            Announce(FName(*FString::Printf(TEXT("Countdown%d"),Count)),TEXT("Countdown"));
        }
    }
    const auto* I=Interaction(); const bool bHeld=I && I->bLocalHeld;
    if (bHeld!=bWasHeld && bPlaying)
    {
        if (bHeld) PlayCue(Cast<ASP1GravityCargo>(I->FocusTarget) ? TEXT("S01Warning") : TEXT("AcquireStart"));
        else if (I && I->LocalReason!=TEXT("Completed")) PlayCue(TEXT("Cancel"));
    }
    bWasHeld=bHeld; PreviousRemaining=Remaining;
}

int32 USP1HUDWidget::NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,
    FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool bParentEnabled) const
{
    const int32 Base=Super::NativePaint(Args,Geometry,Culling,Elements,Layer,Style,bParentEnabled);
    APlayerController* PC=GetOwningPlayer(); const auto* Round=USP1RoundComponent::Find(this);
    if (!bDrawWorldMarkers || !PC || !PC->GetPawn() || !Round) return Base;
    const FVector2D Size=Geometry.GetLocalSize();
    FVector Eye; FRotator Aim; PC->GetPlayerViewPoint(Eye,Aim);
    TArray<FVector2D> Placed;
    auto Marker=[&](FVector World,UTexture2D* Icon,const FString& Label,FLinearColor Color,bool bClamp)
    {
        FVector2D Pos;
        if (!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,World,Pos,true)) return;
        const float Left=360,Right=Size.X-360,Top=170,Bottom=Size.Y-250;
        if (Right<=Left || Bottom<=Top) return;
        if (!bClamp && (Pos.X<Left || Pos.X>Right || Pos.Y<Top || Pos.Y>Bottom)) return;
        Pos.X=FMath::Clamp(Pos.X,Left,Right); Pos.Y=FMath::Clamp(Pos.Y,Top,Bottom);
        for (const auto& P : Placed) if (FMath::Abs(P.X-Pos.X)<120 && FMath::Abs(P.Y-Pos.Y)<38) Pos.Y+=40;
        if (Pos.Y>Bottom) return;
        Placed.Add(Pos);
        const FSlateBrush* Background=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
        FSlateDrawElement::MakeBox(Elements,Base+1,Geometry.ToPaintGeometry(FVector2D(200,34),FSlateLayoutTransform(Pos)),Background,ESlateDrawEffect::None,FLinearColor(0.015f,.025f,.04f,.9f));
        if (Icon)
        {
            FSlateBrush Brush; Brush.SetResourceObject(Icon); Brush.ImageSize=FVector2D(28);
            FSlateDrawElement::MakeBox(Elements,Base+2,Geometry.ToPaintGeometry(FVector2D(28),FSlateLayoutTransform(Pos+FVector2D(3,3))),&Brush,ESlateDrawEffect::None,Color);
        }
        FSlateDrawElement::MakeText(Elements,Base+2,Geometry.ToPaintGeometry(FVector2D(162,28),FSlateLayoutTransform(Pos+FVector2D(36,6))),Label,FCoreStyle::GetDefaultFontStyle("Bold",13),ESlateDrawEffect::None,Color);
    };
    // 표시는 원본 화물 재질에 의존하지 않는다. 완료 표식도 해당 위치에 남기고 근거리 시야로 제한한다.
    for (TActorIterator<ASP1TransferCargo> It(GetWorld());It;++It)
    {
        if (!It->Definition || FVector::DistSquared(Eye,It->GetInteractionPoint())>FMath::Square(1000.f)) continue;
        const FVector Point=It->GetInteractionPoint();
        FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(SP1Marker),false,PC->GetPawn());
        if (GetWorld()->LineTraceSingleByChannel(Hit,Eye,Point,ECC_Visibility,Query) && Hit.GetActor()!=*It) continue;
        const FString Label=It->bTransferred ? TEXT("[완료] 전송됨") : FString::Printf(TEXT("%s %s +%d"),Cast<ASP1GravityCargo>(*It) ? TEXT("[!]") : TEXT("[E]"),*It->Definition->BadgeLabel.ToString(),It->Definition->Value);
        Marker(Point+FVector(0,0,55),It->Definition->Icon,Label,It->bTransferred ? FLinearColor(.65f,.75f,.8f) : It->Definition->BadgeColor,false);
    }
    const FLinearColor Colors[]={FLinearColor(.2f,.9f,1),FLinearColor(1,.7f,.25f),FLinearColor(.65f,.95f,.4f),FLinearColor(.85f,.6f,1)};
    for (const auto& Ping : Round->Pings)
    {
        if (Ping.ExpiresAt<=Round->Now()) continue;
        const int32 Index=Round->Participants.IndexOfByPredicate([&](const auto& P){return P.PlayerId==Ping.PlayerId;});
        const FString Label=FString::Printf(TEXT("P%d %s · %.0fm"),Index+1,Ping.Kind==ESP1PingKind::Danger ? TEXT("위험 !") : Ping.Kind==ESP1PingKind::Cargo ? TEXT("화물") : TEXT("위치"),FVector::Distance(Eye,Ping.Location)/100);
        Marker(Ping.Location,Ping.Kind==ESP1PingKind::Danger ? DangerPingIcon.Get() : Ping.Kind==ESP1PingKind::Cargo ? CargoPingIcon.Get() : LocationPingIcon.Get(),Label,Colors[FMath::Max(0,Index)%4],true);
    }
    for (TActorIterator<APawn> It(GetWorld());It;++It)
    {
        if (*It==PC->GetPawn() || !It->GetPlayerState() || It->IsHidden() || FVector::DistSquared(Eye,It->GetActorLocation())>FMath::Square(1200.f)) continue;
        const int32 Index=Round->Participants.IndexOfByPredicate([&](const auto& P){return P.PlayerState==It->GetPlayerState();});
        if (Index==INDEX_NONE) continue;
        FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(SP1TeammateMarker),false,PC->GetPawn());
        if (GetWorld()->LineTraceSingleByChannel(Hit,Eye,It->GetActorLocation(),ECC_Visibility,Query) && Hit.GetActor()!=*It) continue;
        Marker(It->GetActorLocation()+FVector(0,0,125),nullptr,FString::Printf(TEXT("P%d · 팀원"),Index+1),Colors[Index%4],false);
    }
    return Base+2;
}
