// copyright (c) 2011-2013 the bitcoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#include "macnotificationhandler.h"

#undef slots
#import <objc/runtime.h>
#include <cocoa/cocoa.h>

// add an obj-c category (extension) to return the expected bundle identifier
@implementation nsbundle(returncorrectidentifier)
- (nsstring *)__bundleidentifier
{
    if (self == [nsbundle mainbundle]) {
        return @"org.bitcoinfoundation.bitcoin-qt";
    } else {
        return [self __bundleidentifier];
    }
}
@end

void macnotificationhandler::shownotification(const qstring &title, const qstring &text)
{
    // check if users os has support for nsusernotification
    if(this->hasusernotificationcentersupport()) {
        // okay, seems like 10.8+
        qbytearray utf8 = title.toutf8();
        char* cstring = (char *)utf8.constdata();
        nsstring *titlemac = [[nsstring alloc] initwithutf8string:cstring];

        utf8 = text.toutf8();
        cstring = (char *)utf8.constdata();
        nsstring *textmac = [[nsstring alloc] initwithutf8string:cstring];

        // do everything weak linked (because we will keep <10.8 compatibility)
        id usernotification = [[nsclassfromstring(@"nsusernotification") alloc] init];
        [usernotification performselector:@selector(settitle:) withobject:titlemac];
        [usernotification performselector:@selector(setinformativetext:) withobject:textmac];

        id notificationcenterinstance = [nsclassfromstring(@"nsusernotificationcenter") performselector:@selector(defaultusernotificationcenter)];
        [notificationcenterinstance performselector:@selector(delivernotification:) withobject:usernotification];

        [titlemac release];
        [textmac release];
        [usernotification release];
    }
}

// sendapplescript just take a qstring and executes it as apple script
void macnotificationhandler::sendapplescript(const qstring &script)
{
    qbytearray utf8 = script.toutf8();
    char* cstring = (char *)utf8.constdata();
    nsstring *scriptapple = [[nsstring alloc] initwithutf8string:cstring];

    nsapplescript *as = [[nsapplescript alloc] initwithsource:scriptapple];
    nsdictionary *err = nil;
    [as executeandreturnerror:&err];
    [as release];
    [scriptapple release];
}

bool macnotificationhandler::hasusernotificationcentersupport(void)
{
    class possibleclass = nsclassfromstring(@"nsusernotificationcenter");

    // check if users os has support for nsusernotification
    if(possibleclass!=nil) {
        return true;
    }
    return false;
}


macnotificationhandler *macnotificationhandler::instance()
{
    static macnotificationhandler *s_instance = null;
    if (!s_instance) {
        s_instance = new macnotificationhandler();
        
        class apossibleclass = objc_getclass("nsbundle");
        if (apossibleclass) {
            // change nsbundle -bundleidentifier method to return a correct bundle identifier
            // a bundle identifier is required to use osxs user notification center
            method_exchangeimplementations(class_getinstancemethod(apossibleclass, @selector(bundleidentifier)),
                                           class_getinstancemethod(apossibleclass, @selector(__bundleidentifier)));
        }
    }
    return s_instance;
}
