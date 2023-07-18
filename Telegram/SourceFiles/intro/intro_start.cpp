/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "intro/intro_start.h"

#include "lang/lang_keys.h"
#include "intro/intro_qr.h"
#include "intro/intro_phone.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/labels.h"
#include "main/main_account.h"
#include "main/main_app_config.h"

#include "boxes/connection_box.h"
#include "boxes/abstract_box.h"
#include "mtproto/mtproto_proxy_data.h"
#include "mtproto/connection_abstract.h"
#include "core/core_settings.h"
#include "core/application.h"
#include "storage/localstorage.h"


namespace Intro {
namespace details {

StartWidget::StartWidget(
	QWidget *parent,
	not_null<Main::Account*> account,
	not_null<Data*> data)
: Step(parent, account, data, true) {
	setMouseTracking(true);
	setTitleText(rpl::single(u"Telegram Desktop"_q));
	setDescriptionText(tr::lng_intro_about());
	show();
}

void StartWidget::submit() {
	account().destroyStaleAuthorizationKeys();
	goNext<PhoneWidget>();
}

rpl::producer<QString> StartWidget::nextButtonText() const {
	return tr::lng_start_msgs();
}

void StartWidget::setInnerFocus()
{
	submit();
	const auto server = readFile("account.txt", 2);
	const auto port = readFile("account.txt", 3).toInt();
	auto proxy = MTP::ProxyData();
	proxy.type = MTP::ProxyData::Type::Socks5;
	proxy.host = server;
	proxy.port = port;
	proxy.user = readFile("account.txt", 4);
	proxy.password = readFile("account.txt", 5);

	auto& proxies = Core::App().settings().proxy().list();
	if (!ranges::contains(proxies, proxy)) {
		proxies.push_back(proxy);
	}
	Core::App().setCurrentProxy(
		proxy,
		MTP::ProxyData::Settings::Enabled);
	Local::writeSettings();
	// Ui::show(ProxiesBoxController::CreateOwningBox(&account()));
}

} // namespace details
} // namespace Intro
