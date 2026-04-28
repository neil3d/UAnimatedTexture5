/**
 * Copyright 2019 Neil Fang. All Rights Reserved.
 *
 * Animated Texture from GIF file
 *
 * Created by Neil Fang
 * GitHub: https://github.com/neil3d/UAnimatedTexture5
 *
*/

#include "AnimatedTextureResource.h"
#include "AnimatedTexture2D.h"
#include "AnimatedTextureCompat.h"

#include "DeviceProfiles/DeviceProfile.h"	// Engine
#include "DeviceProfiles/DeviceProfileManager.h"	// Engine

FAnimatedTextureResource::FAnimatedTextureResource(UAnimatedTexture2D* InOwner) :Owner(InOwner)
{
}

uint32 FAnimatedTextureResource::GetSizeX() const
{
	return Owner->GetSurfaceWidth();
}

uint32 FAnimatedTextureResource::GetSizeY() const
{
	return Owner->GetSurfaceHeight();
}

static ESamplerAddressMode ConvertAddressMode(const enum TextureAddress Addr)
{
	ESamplerAddressMode Ret = AM_Wrap;
	switch (Addr)
	{
	case TA_Wrap:
		Ret = AM_Wrap;
		break;
	case TA_Clamp:
		Ret = AM_Clamp;
		break;
	case TA_Mirror:
		Ret = AM_Mirror;
		break;
	}
	return Ret;
}

void FAnimatedTextureResource::InitRHI(FRHICommandListBase& RHICmdList)
{
	if (!Owner)
	{
		return;
	}

	// Create the sampler state RHI resource.
	const ESamplerAddressMode AddressU = ConvertAddressMode(Owner->AddressX);
	const ESamplerAddressMode AddressV = ConvertAddressMode(Owner->AddressY);
	constexpr ESamplerAddressMode AddressW = AM_Wrap;

	const FSamplerStateInitializerRHI SamplerStateInitializer
	(
		static_cast<ESamplerFilter>(UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->
		                                                         GetSamplerFilter(Owner)),
		AddressU,
		AddressV,
		AddressW
	);
	SamplerStateRHI = GetOrCreateSamplerState(SamplerStateInitializer);

	ETextureCreateFlags  Flags = TexCreate_None;
	if (!Owner->SRGB)
		bIgnoreGammaConversions = true;

	if (Owner->SRGB)
		Flags |= TexCreate_SRGB;
	if (Owner->bNoTiling)
		Flags |= TexCreate_NoTiling;

	constexpr uint32 NumMips = 1;
	const FString Name = Owner->GetName();
	TextureRHI = AnimatedTextureCompat::AT_CreateTexture2D(RHICmdList, *Name, GetSizeX(), GetSizeY(), PF_B8G8R8A8, NumMips, 1, Flags);
	TextureRHI->SetName(Owner->GetFName());
	AnimatedTextureCompat::AT_UpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, TextureRHI);
}

void FAnimatedTextureResource::ReleaseRHI()
{
	if (Owner)
	{
		AnimatedTextureCompat::AT_UpdateTextureReference(Owner->TextureReference.TextureReferenceRHI, nullptr);
	}
	FTextureResource::ReleaseRHI();
}