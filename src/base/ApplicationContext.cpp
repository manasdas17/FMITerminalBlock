/* ------------------------------------------------------------------- *
 * Copyright (c) 2017, AIT Austrian Institute of Technology GmbH.      *
 * All rights reserved. See file FMITerminalBlock_LICENSE for details. *
 * ------------------------------------------------------------------- */

/**
 * @file ApplicationContext.cpp
 * @author Michael Spiegel, michael.spiegel@ait.ac.at
 */

#include "base/ApplicationContext.h"
#include "base/BaseExceptions.h"

#include <assert.h>
#include <stdexcept>
#include <string>
#include <sstream>

#include <boost/format.hpp>
#include <boost/optional.hpp>
#include <boost/log/trivial.hpp>
#include <boost/property_tree/info_parser.hpp>

using namespace FMITerminalBlock::Base;

/* Constant Initializations */
const char* const ApplicationContext::PROP_PROGRAM_NAME = "app.name";
const std::string ApplicationContext::PROP_START_TIME = "app.startTime";
const std::string ApplicationContext::PROP_LOOK_AHEAD_TIME = "app.lookAheadTime";
const std::string ApplicationContext::PROP_LOOK_AHEAD_STEP_SIZE = "app.lookAheadStepSize";
const std::string ApplicationContext::PROP_INTEGRATOR_STEP_SIZE = "app.integratorStepSize";
const std::string ApplicationContext::PROP_CHANNEL = "channel";
const std::string ApplicationContext::PROP_OUT_VAR = "out-var";
const std::string ApplicationContext::PROP_IN_VAR = "in-var";
const std::string ApplicationContext::PROP_CONNECTION = "connection";

ApplicationContext::ApplicationContext():
	config_(), outputChannelMap_(NULL), inputChannelMap_(NULL), portIDSource_()
{
	config_.put(PROP_PROGRAM_NAME, "not set");
}

ApplicationContext::ApplicationContext(
	std::initializer_list<std::string> initList):
	config_(), outputChannelMap_(NULL), inputChannelMap_(NULL), portIDSource_()
{
	config_.put(PROP_PROGRAM_NAME, "not set");
	int i = 0;
	for (auto it = initList.begin(); it != initList.end(); ++it)
	{
		addCommandlineOption(*it, i);
		i++;
	}
}

ApplicationContext::~ApplicationContext()
{
	if(outputChannelMap_ != NULL)
	{
		delete outputChannelMap_;
	}
	if (inputChannelMap_ != NULL)
	{
		delete inputChannelMap_;
	}
}

void 
ApplicationContext::addCommandlineProperties(int argc, const char *argv[])
{
	if(argc <= 0 || argv[0] == NULL)
	{
		throw std::invalid_argument("The program name is not set");
	}
	config_.put(PROP_PROGRAM_NAME, argv[0]);

	// Parse commandline arguments
	for(int i = 1; i < argc;i++)
	{
		if(argv[i] == NULL)
		{
			boost::format err("Program option nr. %1$ contains an invalid reference");
			err % i;
			throw std::invalid_argument(err.str());
		}

		std::string opt(argv[i]);

		addCommandlineOption(opt, i);
	}
}

void
ApplicationContext::addCommandlineProperties(
	const std::vector<std::string> &args)
{
	for (int i = 0; i < args.size(); i++)
	{
		addCommandlineOption(args[i], i);
	}
}

void 
ApplicationContext::addCommandlineOption(const std::string &opt, int i)
{
	size_t pos = opt.find_first_of('=');
	if(pos == std::string::npos)
	{
		boost::format err("The program option nr. %1% (\"%2%\") doesn't contain an = sign");
		err % i % opt;
		throw std::invalid_argument(err.str());
	}else if(pos == 0){
		boost::format err("The program option nr. %1% (\"%2%\") doesn't contain a key");
		err % i % opt;
		throw std::invalid_argument(err.str());
	}

	std::string key = opt.substr(0,pos);
	
	if(hasProperty(key))
	{
		boost::format err("The program option nr. %1% (\"%2%\") has already been set with value \"%3%\"");
		err % i % opt % config_.get<std::string>(key);
		throw std::invalid_argument(err.str());
	}

	config_.put(key, opt.substr(pos + 1, opt.size() - pos - 1));

	BOOST_LOG_TRIVIAL(trace) << "Added commandline option \"" << key 
		<< "\" = \"" << config_.get<std::string>(key) << "\"";

}

void 
ApplicationContext::addSensitiveDefaultProperties(const ModelDescription * description)
{
	assert(description != NULL);

	if(!hasProperty(PROP_START_TIME) && description->hasDefaultExperiment())
	{
		fmiReal startTime, stopTime, tolerance, stepSize;
		description->getDefaultExperiment(startTime, stopTime, tolerance, stepSize);
		// Set start time
		config_.put(PROP_START_TIME, std::to_string(startTime));
		BOOST_LOG_TRIVIAL(debug) << "Set start time property " << PROP_START_TIME 
			<< " to the model's default value: " << startTime;
		// TODO: Set default stop time.
	}


}

const ChannelMapping * ApplicationContext::getOutputChannelMapping()
{
	if(outputChannelMap_ == NULL)
	{
		outputChannelMap_ = newChannelMapping(PROP_OUT_VAR);
		BOOST_LOG_TRIVIAL(debug) << "Settled output variable to channel mapping: "
				<< outputChannelMap_->toString();
	}
	return outputChannelMap_;
}

const ChannelMapping * ApplicationContext::getInputChannelMapping()
{
	if (inputChannelMap_ == NULL)
	{
		inputChannelMap_ = newChannelMapping(PROP_IN_VAR);
		BOOST_LOG_TRIVIAL(debug) << "Settled input variable to channel mapping: "
				<< inputChannelMap_->toString();
	}
	return inputChannelMap_;
}

const std::shared_ptr<ApplicationContext::ConnectionConfigMap> 
ApplicationContext::getConnectionConfig()
{
	if (!connections_)
	{ // Parse the configuration and instantiate the map
		std::shared_ptr<ConnectionConfigMap> connections;
		connections = std::make_shared<ConnectionConfigMap>();
		
		addExplicitConnectionConfigs(connections);
		addImplicitConnectionConfigs(connections, getInputChannelMapping());
		addImplicitConnectionConfigs(connections, getOutputChannelMapping());

		checkReferencedConnections(connections, getInputChannelMapping());
		checkReferencedConnections(connections, getOutputChannelMapping());

		connections_ = connections; // Install map, only if fully populated
	}
	return connections_;
}

std::string ApplicationContext::toString() const
{
	std::string ret("ApplicationContext:");
	
	std::ostringstream stream;
	boost::property_tree::info_parser::write_info(stream, config_);
	
	ret += " Configuration: ";
	ret += stream.str();

	ret += " InputChannelMapping: ";
	ret += inputChannelMap_ ? 
		inputChannelMap_->toString() : "<not-constructed>";

	ret += " OutputChannelMapping: ";
	ret += outputChannelMap_ ? 
		outputChannelMap_->toString() : "<not-constructed>";

	return ret;
}

ChannelMapping * 
ApplicationContext::newChannelMapping(const std::string &variablePrefix)
{
	ChannelMapping *channelMap;

	if (hasProperty(PROP_CHANNEL)) {
		channelMap = new ChannelMapping(portIDSource_, 
			getPropertyTree(PROP_CHANNEL), variablePrefix);
	}
	else
	{
		channelMap = new ChannelMapping(portIDSource_);
	}

	return channelMap;
}

void ApplicationContext::addImplicitConnectionConfigs(
	std::shared_ptr<ApplicationContext::ConnectionConfigMap> dest,
	const ChannelMapping* src) const
{
	assert(src);
	assert(dest);

	for (int i = 0; i < src->getNumberOfChannels(); i++)
	{
		const TransmissionChannel& channel = src->getTransmissionChannel(i);
		
		if (dest->count(channel.getConnectionID()) > 0) continue;

		if (channel.isImplicitConnection())
		{
			std::shared_ptr<ConnectionConfig> config;
			config = std::make_shared<ConnectionConfig>(
				channel.getChannelConfig(), channel.getConnectionID());
			dest->insert(std::make_pair(channel.getConnectionID(), config));
		}
	}
}

void ApplicationContext::addExplicitConnectionConfigs(
	std::shared_ptr<ApplicationContext::ConnectionConfigMap> dest) const
{
	assert(dest);
	boost::optional<const boost::property_tree::ptree&> connectionList;
	connectionList = config_.get_child_optional(PROP_CONNECTION);
	if (connectionList)
	{
		for (auto it = connectionList->begin(); it != connectionList->end(); ++it)
		{
			std::string connectionID = it->first;
			assert(dest->count(connectionID) <= 0);

			auto conf = std::make_shared<ConnectionConfig>(it->second, connectionID);
			dest->insert(std::make_pair(connectionID, conf));
		}
	}
}

void ApplicationContext::checkReferencedConnections(
	const std::shared_ptr<ApplicationContext::ConnectionConfigMap> connectionMap,
	const ChannelMapping* channelMap) const
{
	assert(channelMap);
	assert(connectionMap);

	for (int i = 0; i < channelMap->getNumberOfChannels(); i++)
	{
		const TransmissionChannel& channel = channelMap->getTransmissionChannel(i);

		if (channel.isImplicitConnection()) continue;
		if (connectionMap->count(channel.getConnectionID()) <= 0)
		{
			boost::format err("Channel '%1%' references an unknown connection"
				" ('%2%').");
			err % channel.getChannelID() % channel.getConnectionID();
			throw SystemConfigurationException(err.str());
		}
	}
}

std::ostream& FMITerminalBlock::Base::operator<<(std::ostream& stream, 
			const ApplicationContext& appContext)
{
	stream << appContext.toString();
	return stream;
}
