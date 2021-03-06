/**
 * XMPP - libpurple transport
 *
 * Copyright (C) 2009, Jan Kaluza <hanzz@soc.pidgin.im>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "transport/rosterresponder.h"

#include <iostream>
#include <boost/bind.hpp>
#include "Swiften/Queries/IQRouter.h"
#include "Swiften/Swiften.h"
#include "transport/user.h"
#include "transport/usermanager.h"
#include "transport/rostermanager.h"
#include "transport/buddy.h"
#include "transport/logging.h"

using namespace Swift;
using namespace boost;

namespace Transport {

DEFINE_LOGGER(logger, "RosterResponder");

RosterResponder::RosterResponder(Swift::IQRouter *router, UserManager *userManager) : Swift::Responder<RosterPayload>(router) {
	m_userManager = userManager;
}

RosterResponder::~RosterResponder() {
}

bool RosterResponder::handleGetRequest(const Swift::JID& from, const Swift::JID& to, const std::string& id, boost::shared_ptr<Swift::RosterPayload> payload) {
	// Get means we're in server mode and user wants to fetch his roster.
	// For now we send empty reponse, but TODO: Get buddies from database and send proper stored roster.
	User *user = m_userManager->getUser(from.toBare().toString());
	if (!user) {
		sendResponse(from, id, boost::shared_ptr<RosterPayload>(new RosterPayload()));
		LOG4CXX_WARN(logger, from.toBare().toString() << ": User is not logged in");
		return true;
	}
	sendResponse(from, id, user->getRosterManager()->generateRosterPayload());
	user->getRosterManager()->sendCurrentPresences(from);
	return true;
}

bool RosterResponder::handleSetRequest(const Swift::JID& from, const Swift::JID& to, const std::string& id, boost::shared_ptr<Swift::RosterPayload> payload) {
	sendResponse(from, id, boost::shared_ptr<RosterPayload>(new RosterPayload()));

	User *user = m_userManager->getUser(from.toBare().toString());
	if (!user) {
		LOG4CXX_WARN(logger, from.toBare().toString() << ": User is not logged in");
		return true;
	}

	if (payload->getItems().size() == 0) {
		LOG4CXX_WARN(logger, from.toBare().toString() << ": Roster push with no item");
		return true;
	}

	Swift::RosterItemPayload item = payload->getItems()[0];

	Buddy *buddy = user->getRosterManager()->getBuddy(Buddy::JIDToLegacyName(item.getJID()));
	if (buddy) {
		if (item.getSubscription() == Swift::RosterItemPayload::Remove) {
			LOG4CXX_INFO(logger, from.toBare().toString() << ": Removing buddy " << buddy->getName());
			onBuddyRemoved(buddy);

			// send roster push here
			Swift::SetRosterRequest::ref request = Swift::SetRosterRequest::create(payload, user->getJID().toBare(), user->getComponent()->getIQRouter());
			request->send();
		}
		else {
			LOG4CXX_INFO(logger, from.toBare().toString() << ": Updating buddy " << buddy->getName());
			onBuddyUpdated(buddy, item);
		}
	}
	else if (item.getSubscription() != Swift::RosterItemPayload::Remove) {
		// Roster push for this new buddy is sent by RosterManager
		BuddyInfo buddyInfo;
		buddyInfo.id = -1;
		buddyInfo.alias = item.getName();
		buddyInfo.legacyName = Buddy::JIDToLegacyName(item.getJID());
		buddyInfo.subscription = "both";
		buddyInfo.flags = Buddy::buddFlagsFromJID(item.getJID());
		LOG4CXX_INFO(logger, from.toBare().toString() << ": Adding buddy " << buddyInfo.legacyName);

		buddy = user->getComponent()->getFactory()->createBuddy(user->getRosterManager(), buddyInfo);
		user->getRosterManager()->setBuddy(buddy);
		onBuddyAdded(buddy, item);
	}

	return true;
}

}
