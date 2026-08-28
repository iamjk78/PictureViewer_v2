// macOS implementace dokovací nabídky — Objective-C++ soubor.
//
// Qt nemá pro NSApplicationDelegate::applicationDockMenu: žádné cross-platform
// API (a nejde tam ani proto, že jde o čistě macOS koncept). Qt si při startu
// samo vytvoří vlastní delegáta (QCocoaApplicationDelegate) a je jeho jediným
// vlastníkem — nemůžeme mu podstrčit vlastní delegáta, ani ho zdědit.
//
// Qt navíc -applicationDockMenu: SÁM implementuje (ověřeno za běhu — vrací
// prázdné menu, žádné vlastní položky nepřidává), takže prosté „doplnit
// metodu, pokud tam není" (class_addMethod) nic neudělá — AppKit dál volá tu
// Qtovu. Řešení: implementaci vyměníme (method_setImplementation) a tu
// původní zavoláme jako první krok, ať se zachová cokoli, co by Qt v budoucí
// verzi vracet začal.

#include "app/DockMenu.hpp"

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

@interface PVDockMenuTarget : NSObject
- (void)pv_launchNewInstance:(id)sender;
@end

@implementation PVDockMenuTarget
- (void)pv_launchNewInstance:(id)sender
{
    (void)sender;
    // NSWorkspaceOpenConfiguration + createsNewApplicationInstance = přesně
    // chování `open -n` — i když už jedna instance běží, spustí se další
    // místo pouhé aktivace té stávající.
    NSURL *bundleURL = [[NSBundle mainBundle] bundleURL];
    NSWorkspaceOpenConfiguration *config = [NSWorkspaceOpenConfiguration configuration];
    config.createsNewApplicationInstance = YES;
    [[NSWorkspace sharedWorkspace] openApplicationAtURL:bundleURL
                                           configuration:config
                                       completionHandler:nil];
}
@end

namespace {

// Target menu položky — alokován jednou a záměrně nikdy neuvolněn, žije stejně
// dlouho jako NSApp, tedy po celou dobu běhu procesu.
PVDockMenuTarget *g_dockMenuTarget = nil;

// Původní Qt implementace -applicationDockMenu: (viz komentář nahoře) —
// zavolána jako první krok, aby swizzle nic z chování Qt nepřepsal, jen ho
// doplnil.
IMP g_originalDockMenuImp = nullptr;

// Signatura musí přesně odpovídat Objective-C metodě
// -(NSMenu *)applicationDockMenu:(NSApplication *)sender — AppKit ji volá
// přes runtime, ne přes C++ vtable, takže parametry (self, _cmd) jsou povinné.
// Menu se sestavuje znovu při KAŽDÉM otevření (pravý klik / dlouhý stisk
// ikony v Docku), takže žádný stav mezi voláními držet nemusíme.
NSMenu *PVApplicationDockMenu(id self, SEL _cmd, NSApplication *sender)
{
    NSMenu *menu = nil;
    if (g_originalDockMenuImp != nullptr) {
        using OriginalFn = NSMenu *(*)(id, SEL, NSApplication *);
        menu = reinterpret_cast<OriginalFn>(g_originalDockMenuImp)(self, _cmd, sender);
    }
    if (menu == nil) {
        menu = [[[NSMenu alloc] initWithTitle:@""] autorelease];
    }

    if ([menu numberOfItems] > 0) {
        [menu addItem:[NSMenuItem separatorItem]];
    }
    NSMenuItem *item = [[[NSMenuItem alloc] initWithTitle:@"Spustit další"
                                                    action:@selector(pv_launchNewInstance:)
                                             keyEquivalent:@""] autorelease];
    item.target = g_dockMenuTarget;
    [menu addItem:item];
    return menu;
}

} // namespace

namespace pictureviewer {

void setupDockMenu()
{
    if (g_dockMenuTarget == nil) {
        g_dockMenuTarget = [[PVDockMenuTarget alloc] init];
    }

    id delegate = [NSApp delegate];
    if (delegate == nil) {
        return;   // NSApp by v tuto chvíli (po vytvoření QApplication) mělo být hotové
    }

    Class delegateClass = [delegate class];
    SEL selector = @selector(applicationDockMenu:);
    Method existing = class_getInstanceMethod(delegateClass, selector);

    if (existing != nullptr) {
        // Qt metodu implementuje (aktuálně tomu tak je) — vyměnit
        // implementaci, původní zavolat z PVApplicationDockMenu jako první krok.
        g_originalDockMenuImp = method_getImplementation(existing);
        method_setImplementation(existing, reinterpret_cast<IMP>(PVApplicationDockMenu));
    } else {
        class_addMethod(delegateClass, selector,
                        reinterpret_cast<IMP>(PVApplicationDockMenu), "@@:@");
    }
}

} // namespace pictureviewer
