#include "XsollaWebBrowserWrapper.h"

#include "XsollaShop.h"
#include "XsollaSettings.h"
#include "SWebBrowser.h"
#include "Widgets/Layout/SBox.h"
#include "Misc/Base64.h"
#include "XsollaTelegramScheme.h"

#define LOCTEXT_NAMESPACE "XsollaPluginWebBrowserWrapper"

const FSlateBrush* g_SlateCloseBrush;
const FSlateBrush* g_SlateBackBrush;

TSharedPtr<SWebBrowser> g_WebBrowserWidget;

void LoadSlateResources()
{
    TSharedRef<FSlateGameResources> slateButtonResources = FSlateGameResources::New(
        FName("ButtonStyle"),
        "/XsollaPlugin/Buttons",
        "/XsollaPlugin/Buttons"
    );
    FSlateGameResources& buttonStyle = slateButtonResources.Get();
    g_SlateCloseBrush = buttonStyle.GetBrush(FName("close_red_brush"));
    g_SlateBackBrush = buttonStyle.GetBrush(FName("back_brush"));
}

UXsollaWebBrowserWrapper::UXsollaWebBrowserWrapper(const FObjectInitializer& objectInitializer)
    : Super(objectInitializer),
    ButtonSize(GetDefault<UXsollaPluginSettings>()->ButtonSize) // close and home buttons size in Slate units
{
    bIsVariable = true;
}

void UXsollaWebBrowserWrapper::NativeConstruct()
{
    Super::NativeConstruct();
}

void UXsollaWebBrowserWrapper::Open()
{
    ViewportSize = FVector2D(GEngine->GameViewport->Viewport->GetSizeXY());
    ContentSize.Y = ContentSize.Y > ViewportSize.Y ? ViewportSize.Y : ContentSize.Y;

    SAssignNew(WebBrowserWidget, SWebBrowser)
        .InitialURL(InitialURL)
        .ShowControls(false)
        .ViewportSize(ContentSize)
        .SupportsTransparency(bSupportsTransparency)
        .OnUrlChanged(BIND_UOBJECT_DELEGATE(FOnTextChanged, HandleOnUrlChanged))
        .OnLoadCompleted(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnLoadCompleted))
        .OnLoadError(BIND_UOBJECT_DELEGATE(FSimpleDelegate, HandleOnLoadError))
        .OnCloseWindow(BIND_UOBJECT_DELEGATE(FOnCloseWindowDelegate, HandleOnCloseWindow))
        .OnCreateWindow(BIND_UOBJECT_DELEGATE(FOnCreateWindowDelegate, HandleOnPopupCreate))
        .OnSuppressContextMenu_Lambda([]() { return true; });

    ComposeShopWrapper();

    // telegram protocol 
    IWebBrowserModule::Get().GetSingleton()->RegisterSchemeHandlerFactory("tg", "", new TelegramSchemeHandlerFactory());

    PrevFocusedWidget = FSlateApplication::Get().GetUserFocusedWidget(0);
    bPrevGameViewportInputIgnoring = GEngine->GameViewport->IgnoreInput();

    GEngine->GameViewport->SetIgnoreInput(true);
}

void UXsollaWebBrowserWrapper::LoadURL(FString newURL)
{
    if (WebBrowserWidget.IsValid())
    {
        WebBrowserWidget->LoadURL(newURL);
    }
}

void UXsollaWebBrowserWrapper::ComposeShopWrapper()
{
    //BrowserSlot.AttachWidget(SAssignNew(SpinnerImage, SSpinningImage).Image(SlateSpinnerBrush).Period(3.0f));

    BrowserSlot.AttachWidget(SAssignNew(BrowserOverlay, SOverlay));
    BrowserOverlay->AddSlot(0).AttachWidget(WebBrowserWidget.ToSharedRef());

    MainContent = 
        SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight((ViewportSize.Y - ContentSize.Y) / 2)
        + SVerticalBox::Slot().FillHeight(ContentSize.Y)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth((ViewportSize.X - ContentSize.X) / 2)
            + SHorizontalBox::Slot().FillWidth(ContentSize.X + ButtonSize)
            [
                SNew(SHorizontalBox)
                + BrowserSlot.FillWidth(ContentSize.X)
                + SHorizontalBox::Slot().FillWidth(ButtonSize)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
                    [
                        SNew(SBox).MaxDesiredWidth(ButtonSize).MaxDesiredHeight(ButtonSize)
                        [
                            SAssignNew(CloseButton, SButton)
                            .ContentPadding(FMargin(5.0f, 5.0f))
                            .Visibility(EVisibility::Hidden)
                            .ContentPadding(FMargin(0, 0))
                            .ButtonColorAndOpacity(FSlateColor(FLinearColor(0, 0, 0, 0)))
                            .OnClicked_UObject(this, &UXsollaWebBrowserWrapper::CloseShop)
                            .Content()
                            [
                                SNew(SImage)
                                .Image(g_SlateCloseBrush)
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)
                    [
                        SNew(SBox).MaxDesiredWidth(ButtonSize).MaxDesiredHeight(ButtonSize)
                        [
                            SAssignNew(HomeButton, SButton)
                            .ContentPadding(FMargin(5.0f, 5.0f))
                            .Visibility(EVisibility::Hidden)
                            .ContentPadding(FMargin(0, 0))
                            .ButtonColorAndOpacity(FSlateColor(FLinearColor(0, 0, 0, 0)))
                            .OnClicked_UObject(this, &UXsollaWebBrowserWrapper::HandleOnHomeButtonClicked)
                            .Content()
                            [
                                SNew(SImage)
                                .Image(g_SlateBackBrush)
                            ]
                        ]
                    ]
                ]
            ]
            + SHorizontalBox::Slot().FillWidth((ViewportSize.X - ContentSize.X) / 2 - ButtonSize)
        ]
        + SVerticalBox::Slot().FillHeight((ViewportSize.Y - ContentSize.Y) / 2);

    Background = 
        SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                SNew(SColorBlock).Color(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f))
            ]
        ];

    GEngine->GameViewport->AddViewportWidgetContent(MainContent.ToSharedRef(), 9999);
    GEngine->GameViewport->AddViewportWidgetContent(Background.ToSharedRef(), 9998);

    bPrevShowMouseCursor = UGameplayStatics::GetPlayerController(GetWorld(), 0)->bShowMouseCursor;
    UGameplayStatics::GetPlayerController(GetWorld(), 0)->bShowMouseCursor = true;

    //FSlateApplication::Get().SetKeyboardFocus(WebBrowserWidget.ToSharedRef());

    ULocalPlayer* player = GEngine->GetFirstGamePlayer(GEngine->GameViewport);

    if (player != NULL && MainContent.IsValid() && WebBrowserWidget.IsValid())
    {
        player->GetSlateOperations().SetUserFocus(WebBrowserWidget.ToSharedRef());
        //player->GetSlateOperations().LockMouseToWidget(MainContent.ToSharedRef());
        //player->GetSlateOperations().ReleaseMouseCapture();
    }
}

FReply UXsollaWebBrowserWrapper::CloseShop()
{
    if (OnShopClosed.IsBound())
    {
        OnShopClosed.Execute();
    }

    return FReply::Handled();
}

void UXsollaWebBrowserWrapper::HandleOnUrlChanged(const FText& inText)
{
    UE_LOG(LogTemp, Warning, TEXT("New url: %s"), *(WebBrowserWidget->GetUrl()));

    if (!WebBrowserWidget->GetUrl().StartsWith(XsollaPlugin::GetShop()->ApiUrl) && !WebBrowserWidget->GetUrl().StartsWith(XsollaPlugin::GetShop()->SandboxApiUrl))
    {
        HomeButton->SetVisibility(EVisibility::Visible);
    }

    if (WebBrowserWidget->GetUrl().StartsWith(XsollaPlugin::GetShop()->ApiUrl) || WebBrowserWidget->GetUrl().StartsWith(XsollaPlugin::GetShop()->SandboxApiUrl))
    {
        HomeButton->SetVisibility(EVisibility::Hidden);
    }

    OnUrlChanged.Broadcast(inText);

    if (WebBrowserWidget->GetUrl().Contains("www.unrealengine"))
    {
        CloseShop();
    }
}

void UXsollaWebBrowserWrapper::HandleOnLoadCompleted()
{
    CloseButton->SetVisibility(EVisibility::Visible);

    OnLoadCompleted.Broadcast();
}

void UXsollaWebBrowserWrapper::HandleOnLoadError()
{
    OnLoadError.Broadcast();
}

bool UXsollaWebBrowserWrapper::HandleOnCloseWindow(const TWeakPtr<IWebBrowserWindow>& newBrowserWindow)
{
    UE_LOG(LogTemp, Warning, TEXT("Window closed."));

    OnCloseWindow.Broadcast();
    return true;
}

void UXsollaWebBrowserWrapper::SetBrowserSize(float w, float h)
{
    ContentSize.X = w;
    ContentSize.Y = h;
}

void UXsollaWebBrowserWrapper::Clear()
{
    ULocalPlayer* player = GEngine->GetFirstGamePlayer(GEngine->GameViewport);

    if (player != NULL)
    {
        GEngine->GameViewport->SetIgnoreInput(bPrevGameViewportInputIgnoring);

        if (PrevFocusedWidget.IsValid())
        {
            player->GetSlateOperations().SetUserFocus(PrevFocusedWidget.ToSharedRef());
        }

        // player->GetSlateOperations().LockMouseToWidget(PrevFocusedWidget.ToSharedRef());
        player->GetSlateOperations().ReleaseMouseCapture();
        player->GetSlateOperations().ReleaseMouseLock();

        UGameplayStatics::GetPlayerController(GetWorld(), 0)->bShowMouseCursor = bPrevShowMouseCursor;
    }

    if (MainContent.IsValid() && Background.IsValid())
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(MainContent.ToSharedRef());
        GEngine->GameViewport->RemoveViewportWidgetContent(Background.ToSharedRef());
    }

    PopupWidgets.clear();
}

FReply UXsollaWebBrowserWrapper::HandleOnHomeButtonClicked()
{
    if (!PopupWidgets.empty())
    {
        BrowserOverlay->RemoveSlot(PopupWidgets.size());
        PopupWidgets.pop_back();

        if (PopupWidgets.empty())
        {
            if (WebBrowserWidget->GetUrl().StartsWith(XsollaPlugin::GetShop()->ApiUrl) || WebBrowserWidget->GetUrl().StartsWith(XsollaPlugin::GetShop()->SandboxApiUrl))
            {
                HomeButton->SetVisibility(EVisibility::Hidden);
            }
        }

        UE_LOG(LogTemp, Warning, TEXT("Popup closed"));
    }
    else
    {
        this->LoadURL(XsollaPlugin::GetShop()->ShopUrl);
    }

    return FReply::Handled();
}

bool UXsollaWebBrowserWrapper::HandleOnPopupCreate(const TWeakPtr<IWebBrowserWindow>& window, const TWeakPtr<IWebBrowserPopupFeatures>& feat)
{
    UE_LOG(LogTemp, Warning, TEXT("HandleOnPopupCreate()"));

    TSharedPtr<IWebBrowserWindow> popupWindow = window.Pin();
    TSharedPtr<SWebBrowser> popupBrowser = SNew(SWebBrowser);

    SWebBrowser::FArguments args;
    args.ViewportSize(ContentSize);
    args.SupportsTransparency(bSupportsTransparency);
    args.ShowControls(false);
    //args.OnShowDialog_Lambda([](const TWeakPtr<IWebBrowserDialog>& dial) { return EWebBrowserDialogEventResponse::Ignore; });
    args.OnCloseWindow_Lambda([=](const TWeakPtr<IWebBrowserWindow>& win)
    { 
        BrowserOverlay->RemoveSlot(PopupWidgets.size());
        win.Pin()->CloseBrowser(true);
        PopupWidgets.pop_back();

        if (PopupWidgets.empty())
        {
            HomeButton->SetVisibility(EVisibility::Hidden);
        }

        return false; 
    });

    popupBrowser->Construct(args, popupWindow);

    PopupWidgets.push_back(popupBrowser);
    BrowserOverlay->AddSlot(PopupWidgets.size()).AttachWidget(popupBrowser.ToSharedRef());

    HomeButton->SetVisibility(EVisibility::Visible);

    return true;
}

#undef LOCTEXT_NAMESPACE