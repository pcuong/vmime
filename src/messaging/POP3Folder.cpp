//
// VMime library (http://vmime.sourceforge.net)
// Copyright (C) 2002-2004 Vincent Richard <vincent@vincent-richard.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
//

#include "POP3Folder.hpp"

#include "POP3Store.hpp"
#include "POP3Message.hpp"

#include "../exception.hpp"


namespace vmime {
namespace messaging {


POP3Folder::POP3Folder(const folder::path& path, POP3Store* store)
	: m_store(store), m_path(path), m_name(path.last()), m_mode(-1), m_open(false)
{
	m_store->registerFolder(this);
}


POP3Folder::~POP3Folder()
{
	if (m_store)
	{
		if (m_open)
			close(false);

		m_store->unregisterFolder(this);
	}
	else if (m_open)
	{
		onClose();
	}
}


const int POP3Folder::mode() const
{
	if (!isOpen())
		throw exceptions::illegal_state("Folder not open");

	return (m_mode);
}


const int POP3Folder::type()
{
	if (!isOpen())
		throw exceptions::illegal_state("Folder not open");

	if (m_path.empty())
		return (TYPE_CONTAINS_FOLDERS);
	else if (m_path.size() == 1 && m_path[0].buffer() == "INBOX")
		return (TYPE_CONTAINS_MESSAGES);
	else
		throw exceptions::folder_not_found();
}


const int POP3Folder::flags()
{
	return (0);
}


const folder::path::component POP3Folder::name() const
{
	return (m_name);
}


const folder::path POP3Folder::fullPath() const
{
	return (m_path);
}


void POP3Folder::open(const int mode, bool failIfModeIsNotAvailable)
{
	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");

	if (m_path.empty())
	{
		if (mode != MODE_READ_ONLY && failIfModeIsNotAvailable)
			throw exceptions::operation_not_supported();

		m_open = true;
		m_mode = mode;

		m_messageCount = 0;
	}
	else if (m_path.size() == 1 && m_path[0].buffer() == "INBOX")
	{
		m_store->sendRequest("STAT");

		string response;
		m_store->readResponse(response, false);

		if (!m_store->isSuccessResponse(response))
			throw exceptions::command_error("STAT", response);

		m_store->stripResponseCode(response, response);

		std::istringstream iss(response);
		iss >> m_messageCount;

		if (iss.fail())
			throw exceptions::invalid_response("STAT", response);

		m_open = true;
		m_mode = mode;
	}
	else
	{
		throw exceptions::folder_not_found();
	}
}

void POP3Folder::close(const bool expunge)
{
	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");

	if (!isOpen())
		throw exceptions::illegal_state("Folder not open");

	if (!expunge)
	{
		m_store->sendRequest("RSET");

		string response;
		m_store->readResponse(response, false);
	}

	m_open = false;
	m_mode = -1;

	onClose();
}


void POP3Folder::onClose()
{
	for (MessageMap::iterator it = m_messages.begin() ; it != m_messages.end() ; ++it)
		(*it).first->onFolderClosed();

	m_messages.clear();
}


void POP3Folder::create(const int /* type */)
{
	throw exceptions::operation_not_supported();
}


const bool POP3Folder::exists()
{
	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");

	return (m_path.empty() || (m_path.size() == 1 && m_path[0].buffer() == "INBOX"));
}


const bool POP3Folder::isOpen() const
{
	return (m_open);
}


message* POP3Folder::getMessage(const int num)
{
	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");
	else if (!isOpen())
		throw exceptions::illegal_state("Folder not open");
	else if (num < 1 || num > m_messageCount)
		throw exceptions::message_not_found();

	return new POP3Message(this, num);
}


std::vector <message*> POP3Folder::getMessages(const int from, const int to)
{
	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");
	else if (!isOpen())
		throw exceptions::illegal_state("Folder not open");
	else if (to < from || from < 1 || to < 1 || from > m_messageCount || to > m_messageCount)
		throw exceptions::message_not_found();

	std::vector <message*> v;

	for (int i = from ; i <= to ; ++i)
		v.push_back(new POP3Message(this, i));

	return (v);
}


std::vector <message*> POP3Folder::getMessages(const std::vector <int>& nums)
{
	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");
	else if (!isOpen())
		throw exceptions::illegal_state("Folder not open");

	std::vector <message*> v;

	try
	{
		for (std::vector <int>::const_iterator it = nums.begin() ; it != nums.end() ; ++it)
		{
			if (*it < 1|| *it > m_messageCount)
				throw exceptions::message_not_found();

			v.push_back(new POP3Message(this, *it));
		}
	}
	catch (std::exception& e)
	{
		for (std::vector <message*>::iterator it = v.begin() ; it != v.end() ; ++it)
			delete (*it);

		throw;
	}

	return (v);
}


const int POP3Folder::getMessageCount()
{
	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");
	else if (!isOpen())
		throw exceptions::illegal_state("Folder not open");

	return (m_messageCount);
}


folder* POP3Folder::getFolder(const folder::path::component& name)
{
	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");

	return new POP3Folder(m_path / name, m_store);
}


std::vector <folder*> POP3Folder::getFolders(const bool /* recursive */)
{
	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");

	if (m_path.empty())
	{
		std::vector <folder*> v;
		v.push_back(new POP3Folder(folder::path::component("INBOX"), m_store));
		return (v);
	}
	else
	{
		std::vector <folder*> v;
		return (v);
	}
}


void POP3Folder::fetchMessages(std::vector <message*>& msg, const int options,
                               progressionListener* progress)
{
	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");
	else if (!isOpen())
		throw exceptions::illegal_state("Folder not open");

	const int total = msg.size();
	int current = 0;

	if (progress)
		progress->start(total);

	for (std::vector <message*>::iterator it = msg.begin() ;
	     it != msg.end() ; ++it)
	{
		dynamic_cast <POP3Message*>(*it)->fetch(this, options);

		if (progress)
			progress->progress(++current, total);
	}

	if (options & FETCH_UID)
	{
		// Send the "UIDL" command
		std::ostringstream command;
		command << "UIDL";

		m_store->sendRequest(command.str());

		// Get the response
		string response;
		m_store->readResponse(response, true, NULL);

		if (m_store->isSuccessResponse(response))
		{
			m_store->stripFirstLine(response, response, NULL);

			// C: UIDL
			// S: +OK
			// S: 1 whqtswO00WBw418f9t5JxYwZ
			// S: 2 QhdPYR:00WBw1Ph7x7
			// S: .

			std::istringstream iss(response);
			std::map <int, string> ids;

			string line;

			while (std::getline(iss, line))
			{
				string::iterator it = line.begin();

				while (it != line.end() && (*it == ' ' || *it == '\t'))
					++it;

				if (it != line.end())
				{
					int number = 0;

					while (it != line.end() && (*it >= '0' && *it <= '9'))
					{
						number = (number * 10) + (*it - '0');
						++it;
					}

					while (it != line.end() && !(*it == ' ' || *it == '\t')) ++it;
					while (it != line.end() && (*it == ' ' || *it == '\t')) ++it;

					if (it != line.end())
					{
						ids.insert(std::map <int, string>::value_type
							(number, string(it, line.end())));
					}
				}
			}

			for (std::vector <message*>::iterator it = msg.begin() ;
			     it != msg.end() ; ++it)
			{
				POP3Message* m = dynamic_cast <POP3Message*>(*it);

				std::map <int, string>::const_iterator id =
					ids.find(m->m_num);

				if (id != ids.end())
					m->m_uid = (*id).second;
			}
		}
	}

	if (progress)
		progress->stop(total);
}


void POP3Folder::fetchMessage(message* msg, const int options)
{
	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");
	else if (!isOpen())
		throw exceptions::illegal_state("Folder not open");

	dynamic_cast <POP3Message*>(msg)->fetch(this, options);

	if (options & FETCH_UID)
	{
		// Send the "UIDL" command
		std::ostringstream command;
		command << "UIDL";

		m_store->sendRequest(command.str());

		// Get the response
		string response;
		m_store->readResponse(response, false, NULL);

		if (m_store->isSuccessResponse(response))
		{
			m_store->stripResponseCode(response, response);

			// C: UIDL 2
			// S: +OK 2 QhdPYR:00WBw1Ph7x7
			string::iterator it = response.begin();

			while (it != response.end() && (*it == ' ' || *it == '\t')) ++it;
			while (it != response.end() && !(*it == ' ' || *it == '\t')) ++it;
			while (it != response.end() && (*it == ' ' || *it == '\t')) ++it;

			if (it != response.end())
			{
				dynamic_cast <POP3Message*>(msg)->m_uid =
					string(it, response.end());
			}
		}
	}
}


const int POP3Folder::getFetchCapabilities() const
{
	return (FETCH_ENVELOPE | FETCH_CONTENT_INFO |
	        FETCH_SIZE | FETCH_FULL_HEADER | FETCH_UID);
}


folder* POP3Folder::getParent()
{
	return (m_path.empty() ? NULL : new POP3Folder(m_path.parent(), m_store));
}


const class store& POP3Folder::store() const
{
	return (*m_store);
}


class store& POP3Folder::store()
{
	return (*m_store);
}


void POP3Folder::registerMessage(POP3Message* msg)
{
	m_messages.insert(MessageMap::value_type(msg, msg->number()));
}


void POP3Folder::unregisterMessage(POP3Message* msg)
{
	m_messages.erase(msg);
}


void POP3Folder::onStoreDisconnected()
{
	m_store = NULL;
}


void POP3Folder::deleteMessage(const int num)
{
	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");
	else if (!isOpen())
		throw exceptions::illegal_state("Folder not open");

	std::ostringstream command;
	command << "DELE " << num;

	m_store->sendRequest(command.str());

	string response;
	m_store->readResponse(response, false);

	if (!m_store->isSuccessResponse(response))
		throw exceptions::command_error("DELE", response);
}


void POP3Folder::deleteMessages(const int from, const int to)
{
	if (from < 1 || (to < from && to != -1))
		throw exceptions::invalid_argument();

	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");
	else if (!isOpen())
		throw exceptions::illegal_state("Folder not open");

	for (int i = from ; i < to ; ++i)
	{
		std::ostringstream command;
		command << "DELE " << i;

		m_store->sendRequest(command.str());

		string response;
		m_store->readResponse(response, false);

		if (!m_store->isSuccessResponse(response))
			throw exceptions::command_error("DELE", response);
	}
}


void POP3Folder::deleteMessages(const std::vector <int>& nums)
{
	if (nums.empty())
		throw exceptions::invalid_argument();

	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");
	else if (!isOpen())
		throw exceptions::illegal_state("Folder not open");

	for (std::vector <int>::const_iterator
	     it = nums.begin() ; it != nums.end() ; ++it)
	{
		std::ostringstream command;
		command << "DELE " << (*it);

		m_store->sendRequest(command.str());

		string response;
		m_store->readResponse(response, false);

		if (!m_store->isSuccessResponse(response))
			throw exceptions::command_error("DELE", response);
	}
}


void POP3Folder::setMessageFlags(const int /* from */, const int /* to */,
	const int /* flags */, const int /* mode */)
{
	throw exceptions::operation_not_supported();
}


void POP3Folder::setMessageFlags(const std::vector <int>& /* nums */,
	const int /* flags */, const int /* mode */)
{
	throw exceptions::operation_not_supported();
}


void POP3Folder::rename(const folder::path& /* newPath */)
{
	throw exceptions::operation_not_supported();
}


void POP3Folder::addMessage(vmime::message* /* msg */, const int /* flags */,
	vmime::datetime* /* date */, progressionListener* /* progress */)
{
	throw exceptions::operation_not_supported();
}


void POP3Folder::addMessage(utility::inputStream& /* is */, const int /* size */, const int /* flags */,
	vmime::datetime* /* date */, progressionListener* /* progress */)
{
	throw exceptions::operation_not_supported();
}


void POP3Folder::copyMessage(const folder::path& /* dest */, const int /* num */)
{
	throw exceptions::operation_not_supported();
}


void POP3Folder::copyMessages(const folder::path& /* dest */, const int /* from */, const int /* to */)
{
	throw exceptions::operation_not_supported();
}


void POP3Folder::copyMessages(const folder::path& /* dest */, const std::vector <int>& /* nums */)
{
	throw exceptions::operation_not_supported();
}


void POP3Folder::status(int& count, int& unseen)
{
	if (!m_store)
		throw exceptions::illegal_state("Store disconnected");
	else if (!isOpen())
		throw exceptions::illegal_state("Folder not open");

	m_store->sendRequest("STAT");

	string response;
	m_store->readResponse(response, false);

	if (!m_store->isSuccessResponse(response))
		throw exceptions::command_error("STAT", response);

	m_store->stripResponseCode(response, response);

	std::istringstream iss(response);
	iss >> count;

	unseen = count;

	// Update local message count
	if (m_messageCount != count)
	{
		const int oldCount = m_messageCount;

		m_messageCount = count;

		if (count > oldCount)
		{
			std::vector <int> nums;
			nums.reserve(count - oldCount);

			for (int i = oldCount + 1, j = 0 ; i <= count ; ++i, ++j)
				nums[j] = i;

			events::messageCountEvent event(this, events::messageCountEvent::TYPE_ADDED, nums);

			for (std::list <POP3Folder*>::iterator it = m_store->m_folders.begin() ;
			     it != m_store->m_folders.end() ; ++it)
			{
				if ((*it)->fullPath() == m_path)
				{
					(*it)->m_messageCount = count;
					(*it)->notifyMessageCount(event);
				}
			}
		}
	}
}


void POP3Folder::expunge()
{
	// Not supported by POP3 protocol (deleted messages are automatically
	// expunged at the end of the session...).
}


} // messaging
} // vmime
