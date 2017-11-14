// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "XsollaPluginBPLibrary.h"
#include "XsollaPlugin.h"
#include "XsollaPluginSettings.h"
#include "XsollaPluginWebBrowser.h"
#include "Misc/Base64.h"

UXsollaPluginBPLibrary::UXsollaPluginBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	HttpTool = new XsollaPluginHttpTool();

	// xsolla api stuffs
	bIsSandbox = GetDefault<UXsollaPluginSettings>()->bSandboxMode;
	if (bIsSandbox)
	{
		ShopUrl = "https://sandbox-secure.xsolla.com/paystation2/?access_token=";
	}
	else
	{
		ShopUrl = "https://secure.xsolla.com/paystation2/?access_token=";
	}

	MerchantId = GetDefault<UXsollaPluginSettings>()->MerchantId;
	ProjectId = GetDefault<UXsollaPluginSettings>()->ProjectId;
	ApiKey = GetDefault<UXsollaPluginSettings>()->ApiKey;
}

int32 UXsollaPluginBPLibrary::CreateXsollaShop(FOnPaymantSucceeded OnSucceeded, FOnPaymantCanceled OnCanceled, FOnPaymantFailed OnFailed)
{
	/* SHOP JSON */
	// user section
	TSharedPtr<FJsonObject> userIdJsonObj = MakeShareable(new FJsonObject);
	userIdJsonObj->SetStringField("value", "1234567");
	userIdJsonObj->SetBoolField("hidden", true);

	TSharedPtr<FJsonObject> userJsonObj = MakeShareable(new FJsonObject);
	userJsonObj->SetObjectField("id", userIdJsonObj);

	TSharedPtr<FJsonObject> emailJsonObj = MakeShareable(new FJsonObject);
	emailJsonObj->SetStringField("value", "example@example.com");
	emailJsonObj->SetBoolField("allow_modify", true);
	emailJsonObj->SetBoolField("hidden", false);

	userJsonObj->SetObjectField("email", emailJsonObj);

	// settings section
	TSharedPtr<FJsonObject> settingsJsonObj = MakeShareable(new FJsonObject);
	settingsJsonObj->SetNumberField("project_id", FCString::Atoi(*ProjectId));
	if (bIsSandbox)
	{
		settingsJsonObj->SetStringField("mode", "sandbox");
	}

	// combine into main section
	TSharedPtr<FJsonObject> requestJsonObj = MakeShareable(new FJsonObject);
	requestJsonObj->SetObjectField("user", userJsonObj);
	requestJsonObj->SetObjectField("settings", settingsJsonObj);

	// get string from json
	FString outputString;
	TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&outputString);
	FJsonSerializer::Serialize(requestJsonObj.ToSharedRef(), Writer);

	// get shop token
	GetToken(outputString);

	return -1;
}

void UXsollaPluginBPLibrary::GetToken(FString shopJson)
{
	FString route = MerchantId;
	route += "/token";

	TSharedRef<IHttpRequest> Request = HttpTool->PostRequest(route, shopJson);

	Request->OnProcessRequestComplete().BindUObject(this, &UXsollaPluginBPLibrary::OnLoadResponse);
	HttpTool->SetAuthorizationHash(FString("Basic ") + FBase64::Encode(MerchantId + FString(":") + ApiKey), Request);

	HttpTool->Send(Request);
}

void UXsollaPluginBPLibrary::OnLoadResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!HttpTool->ResponseIsValid(Response, bWasSuccessful))
		return;

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Response->GetContentAsString());

	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed))
	{
		FString tokenString = JsonParsed->GetStringField("token");
		ShopUrl += tokenString;
		XsollaToken = tokenString;
		UE_LOG(LogTemp, Warning, TEXT("token: %s"), *ShopUrl);
	}
}

