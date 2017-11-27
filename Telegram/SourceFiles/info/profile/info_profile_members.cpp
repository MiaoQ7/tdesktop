/*
This file is part of Telegram Desktop,
the official desktop version of Telegram messaging app, see https://telegram.org

Telegram Desktop is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

It is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

In addition, as a special exception, the copyright holders give permission
to link the code of portions of this program with the OpenSSL library.

Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
Copyright (c) 2014-2017 John Preston, https://desktop.telegram.org
*/
#include "info/profile/info_profile_members.h"

#include <rpl/combine.h>
#include "info/profile/info_profile_widget.h"
#include "info/profile/info_profile_values.h"
#include "info/profile/info_profile_icon.h"
#include "info/profile/info_profile_values.h"
#include "info/profile/info_profile_button.h"
#include "info/profile/info_profile_members_controllers.h"
#include "info/info_content_widget.h"
#include "info/info_controller.h"
#include "info/info_memento.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/input_fields.h"
#include "ui/widgets/scroll_area.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/search_field_controller.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h"
#include "lang/lang_keys.h"
#include "boxes/confirm_box.h"
#include "boxes/peer_list_controllers.h"
#include "window/window_controller.h"

namespace Info {
namespace Profile {
namespace {

constexpr auto kEnableSearchMembersAfterCount = 20;

} // namespace

Members::Members(
	QWidget *parent,
	not_null<Controller*> controller,
	not_null<PeerData*> peer)
: RpWidget(parent)
, _controller(controller)
, _peer(peer)
, _listController(CreateMembersController(controller->window(), _peer)) {
	setupHeader();
	setupList();
	setContent(_list.data());
	_listController->setDelegate(static_cast<PeerListDelegate*>(this));

	_controller->searchFieldController()->queryValue()
		| rpl::start_with_next([this](QString &&query) {
			peerListScrollToTop();
			content()->searchQueryChanged(std::move(query));
		}, lifetime());
}

int Members::desiredHeight() const {
	auto desired = _header ? _header->height() : 0;
	auto count = [this] {
		if (auto chat = _peer->asChat()) {
			return chat->count;
		} else if (auto channel = _peer->asChannel()) {
			return channel->membersCount();
		}
		return 0;
	}();
	desired += qMax(count, _list->fullRowsCount())
		* st::infoMembersList.item.height;
	return qMax(height(), desired);
}

rpl::producer<int> Members::onlineCountValue() const {
	return _listController->onlineCountValue();
}

rpl::producer<Ui::ScrollToRequest> Members::scrollToRequests() const {
	return _scrollToRequests.events();
}

std::unique_ptr<MembersState> Members::saveState() {
	auto result = std::make_unique<MembersState>();
	result->list = _listController->saveState();
	//if (_searchShown) {
	//	result->search = _searchField->getLastText();
	//}
	return result;
}

void Members::restoreState(std::unique_ptr<MembersState> state) {
	if (!state) {
		return;
	}
	_listController->restoreState(std::move(state->list));
	updateSearchEnabledByContent();
	//if (!_controller->searchFieldController()->query().isEmpty()) {
	//	if (!_searchShown) {
	//		toggleSearch(anim::type::instant);
	//	}
	//} else if (_searchShown) {
	//	toggleSearch(anim::type::instant);
	//}
}

void Members::setupHeader() {
	if (_controller->section().type() == Section::Type::Members) {
		return;
	}
	_header = object_ptr<Ui::FixedHeightWidget>(
		this,
		st::infoMembersHeader);
	auto parent = _header.data();

	_openMembers = Ui::CreateChild<Button>(
		parent,
		rpl::single(QString()));

	object_ptr<FloatingIcon>(
		parent,
		st::infoIconMembers,
		st::infoIconPosition);

	_titleWrap = Ui::CreateChild<Ui::RpWidget>(parent);
	_title = setupTitle();
	_addMember = Ui::CreateChild<Ui::IconButton>(
		_openMembers,
		st::infoMembersAddMember);
	//_searchField = _controller->searchFieldController()->createField(
	//	parent,
	//	st::infoMembersSearchField);
	//_search = Ui::CreateChild<Ui::IconButton>(
	//	parent,
	//	st::infoMembersSearch);
	//_cancelSearch = Ui::CreateChild<Ui::CrossButton>(
	//	parent,
	//	st::infoMembersCancelSearch);

	setupButtons();

	//_controller->wrapValue()
	//	| rpl::start_with_next([this](Wrap wrap) {
	//		_wrap = wrap;
	//		updateSearchOverrides();
	//	}, lifetime());
	widthValue()
		| rpl::start_with_next([this](int width) {
			_header->resizeToWidth(width);
		}, _header->lifetime());
}

object_ptr<Ui::FlatLabel> Members::setupTitle() {
	auto result = object_ptr<Ui::FlatLabel>(
		_titleWrap,
		MembersCountValue(_peer)
			| rpl::map([](int count) {
				return lng_chat_status_members(lt_count, count);
			})
			| ToUpperValue(),
		st::infoBlockHeaderLabel);
	result->setAttribute(Qt::WA_TransparentForMouseEvents);
	return result;
}

void Members::setupButtons() {
	using namespace rpl::mappers;

	_openMembers->addClickHandler([this] {
		_controller->window()->showSection(Info::Memento(
			_controller->peerId(),
			Section::Type::Members));
	});

	//_searchField->hide();
	//_cancelSearch->setVisible(false);

	auto addMemberShown = CanAddMemberValue(_peer)
		| rpl::start_spawning(lifetime());
	_addMember->showOn(rpl::duplicate(addMemberShown));
	_addMember->addClickHandler([this] { // TODO throttle(ripple duration)
		this->addMember();
	});

	//auto searchShown = MembersCountValue(_peer)
	//	| rpl::map(_1 >= kEnableSearchMembersAfterCount)
	//	| rpl::distinct_until_changed()
	//	| rpl::start_spawning(lifetime());
	//_search->showOn(rpl::duplicate(searchShown));
	//_search->addClickHandler([this] {
	//	this->showSearch();
	//});
	//_cancelSearch->addClickHandler([this] {
	//	this->cancelSearch();
	//});

	//rpl::combine(
	//	std::move(addMemberShown),
	//	std::move(searchShown))
	std::move(addMemberShown)
		| rpl::start_with_next([this] {
			updateHeaderControlsGeometry(width());
		}, lifetime());
}

void Members::setupList() {
	auto topSkip = _header ? _header->height() : 0;
	_list = object_ptr<ListWidget>(
		this,
		_listController.get(),
		st::infoMembersList);
	_list->scrollToRequests()
		| rpl::start_with_next([this](Ui::ScrollToRequest request) {
			auto addmin = (request.ymin < 0 || !_header)
				? 0
				: _header->height();
			auto addmax = (request.ymax < 0 || !_header)
				? 0
				: _header->height();
			_scrollToRequests.fire({
				request.ymin + addmin,
				request.ymax + addmax });
		}, _list->lifetime());
	widthValue()
		| rpl::start_with_next([this](int newWidth) {
			_list->resizeToWidth(newWidth);
		}, _list->lifetime());
	_list->heightValue()
		| rpl::start_with_next([=](int listHeight) {
			auto newHeight = (listHeight > st::membersMarginBottom)
				? (topSkip
					+ listHeight
					+ st::membersMarginBottom)
				: 0;
			resize(width(), newHeight);
		}, _list->lifetime());
	_list->moveToLeft(0, topSkip);
}

int Members::resizeGetHeight(int newWidth) {
	if (_header) {
		updateHeaderControlsGeometry(newWidth);
	}
	return heightNoMargins();
}

void Members::updateSearchEnabledByContent() {
	_controller->setSearchEnabledByContent(
		peerListFullRowsCount() >= kEnableSearchMembersAfterCount);
}

void Members::updateHeaderControlsGeometry(int newWidth) {
	_openMembers->setGeometry(0, st::infoProfileSkip, newWidth, st::infoMembersHeader - st::infoProfileSkip - st::infoMembersHeaderPaddingBottom);

	auto availableWidth = newWidth
		- st::infoMembersButtonPosition.x();

	//auto cancelLeft = availableWidth - _cancelSearch->width();
	//_cancelSearch->moveToLeft(
	//	cancelLeft,
	//	st::infoMembersButtonPosition.y());

	//auto searchShownLeft = st::infoIconPosition.x()
	//	- st::infoMembersSearch.iconPosition.x();
	//auto searchHiddenLeft = availableWidth - _search->width();
	//auto searchShown = _searchShownAnimation.current(_searchShown ? 1. : 0.);
	//auto searchCurrentLeft = anim::interpolate(
	//	searchHiddenLeft,
	//	searchShownLeft,
	//	searchShown);
	//_search->moveToLeft(
	//	searchCurrentLeft,
	//	st::infoMembersButtonPosition.y());

	//if (!_search->isHidden()) {
	//	availableWidth -= st::infoMembersSearch.width;
	//}
	_addMember->moveToLeft(
		availableWidth - _addMember->width(),
		st::infoMembersButtonPosition.y(),
		newWidth);

	//auto fieldLeft = anim::interpolate(
	//	cancelLeft,
	//	st::infoBlockHeaderPosition.x(),
	//	searchShown);
	//_searchField->setGeometryToLeft(
	//	fieldLeft,
	//	st::infoMembersSearchTop,
	//	cancelLeft - fieldLeft,
	//	_searchField->height());

	//_titleWrap->resize(
	//	searchCurrentLeft - st::infoBlockHeaderPosition.x(),
	//	_title->height());
	_titleWrap->resize(
		availableWidth - _addMember->width() - st::infoBlockHeaderPosition.x(),
		_title->height());
	_titleWrap->moveToLeft(
		st::infoBlockHeaderPosition.x(),
		st::infoBlockHeaderPosition.y(),
		newWidth);
	_titleWrap->setAttribute(Qt::WA_TransparentForMouseEvents);

	//_title->resizeToWidth(searchHiddenLeft);
	_title->resizeToWidth(_titleWrap->width());
	_title->moveToLeft(0, 0);
}

void Members::addMember() {
	if (auto chat = _peer->asChat()) {
		if (chat->count >= Global::ChatSizeMax() && chat->amCreator()) {
			Ui::show(Box<ConvertToSupergroupBox>(chat));
		} else {
			AddParticipantsBoxController::Start(chat);
		}
	} else if (auto channel = _peer->asChannel()) {
		if (channel->mgInfo) {
			auto &participants = channel->mgInfo->lastParticipants;
			AddParticipantsBoxController::Start(channel, { participants.cbegin(), participants.cend() });
		}
	}
}

//void Members::showSearch() {
//	if (!_searchShown) {
//		toggleSearch();
//	}
//}
//
//void Members::toggleSearch(anim::type animated) {
//	_searchShown = !_searchShown;
//	_cancelSearch->toggle(_searchShown, animated);
//	if (animated == anim::type::normal) {
//		_searchShownAnimation.start(
//			[this] { searchAnimationCallback(); },
//			_searchShown ? 0. : 1.,
//			_searchShown ? 1. : 0.,
//			st::slideWrapDuration);
//	} else {
//		_searchShownAnimation.finish();
//		searchAnimationCallback();
//	}
//	_search->setDisabled(_searchShown);
//	if (_searchShown) {
//		_searchField->show();
//		_searchField->setFocus();
//	} else {
//		setFocus();
//	}
//}
//
//void Members::searchAnimationCallback() {
//	if (!_searchShownAnimation.animating()) {
//		_searchField->setVisible(_searchShown);
//		updateSearchOverrides();
//		_search->setPointerCursor(!_searchShown);
//	}
//	updateHeaderControlsGeometry(width());
//}
//
//void Members::updateSearchOverrides() {
//	auto iconOverride = !_searchShown
//		? nullptr
//		: (_wrap == Wrap::Layer)
//		? &st::infoMembersSearchActiveLayer
//		: &st::infoMembersSearchActive;
//	_search->setIconOverride(iconOverride, iconOverride);
//}
//
//void Members::cancelSearch() {
//	if (_searchShown) {
//		if (!_searchField->getLastText().isEmpty()) {
//			_searchField->setText(QString());
//			_searchField->setFocus();
//		} else {
//			toggleSearch();
//		}
//	}
//}

void Members::visibleTopBottomUpdated(
		int visibleTop,
		int visibleBottom) {
	setChildVisibleTopBottom(_list, visibleTop, visibleBottom);
}

void Members::peerListSetTitle(base::lambda<QString()> title) {
}

void Members::peerListSetAdditionalTitle(
		base::lambda<QString()> title) {
}

bool Members::peerListIsRowSelected(not_null<PeerData*> peer) {
	return false;
}

int Members::peerListSelectedRowsCount() {
	return 0;
}

std::vector<not_null<PeerData*>> Members::peerListCollectSelectedRows() {
	return {};
}

void Members::peerListScrollToTop() {
	_scrollToRequests.fire({ -1, -1 });
}

void Members::peerListAddSelectedRowInBunch(not_null<PeerData*> peer) {
	Unexpected("Item selection in Info::Profile::Members.");
}

void Members::peerListFinishSelectedRowsBunch() {
}

void Members::peerListSetDescription(
		object_ptr<Ui::FlatLabel> description) {
	description.destroy();
}

} // namespace Profile
} // namespace Info

