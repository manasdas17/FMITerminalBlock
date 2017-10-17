/* ------------------------------------------------------------------- *
 * Copyright (c) 2017, AIT Austrian Institute of Technology GmbH.      *
 * All rights reserved. See file FMITerminalBlock_LICENSE for details. *
 * ------------------------------------------------------------------- */

/**
 * @file ApplicationContext.h
 * @author Michael Spiegel, michael.spiegel@ait.ac.at
 */

#ifndef _FMITERMINALBLOCK_BASE_APPLICATION_CONTEXT
#define _FMITERMINALBLOCK_BASE_APPLICATION_CONTEXT

#include <base/ChannelMapping.h>
#include <base/AbstractConfigProvider.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/format.hpp>
// Fixes an include dependency flaw/feature(?) of ModelDescription.h
#include <common/fmi_v1.0/fmiModelTypes.h>
#include <import/base/include/ModelDescription.h>
#include <string>
#include <iostream>

/**
 * @brief returns the number of arguments in a valid argument vector array.
 * @details It is assumed that the argument vector array argv is null 
 * terminated. Hence, one element will be subtracted from the total count of 
 * elements. The macro is mainly intended for testing purpose. 
 */
#define ARG_NUM_OF_ARGV( argv ) (sizeof((argv)) / sizeof((argv)[0]) - 1)

namespace FMITerminalBlock
{
	namespace Base
	{

		/**
		 * @brief Utility class which provides some application scoped information
		 * and functionality.
		 * @details <p>It encapsulates the configuration structure as well as some
		 * commonly used functionality. It provides a simple interface to retrieve
		 * and check configuration values and to obtain the channel mapping. The
		 * ApplicationContext class is intended to be passed to all program modules
		 * which require a dynamic configuration.</p>
		 * <p> In not state otherwise, function's which return a property's value or
		 * a subtree access the global configuration which is maintained by the
		 * ApplicationContext object. In general, properties are accessed via the
		 * default path identifier. Each hierarchic level in the property tree is
		 * separated by a single dot character.
		 * </p>
		 */
		class ApplicationContext: public AbstractConfigProvider
		{
		public:

			/** @brief The key of the program-name property */
			static const std::string PROP_PROGRAM_NAME;

			/** @brief The key of the start time property */
			static const std::string PROP_START_TIME;

			/** @brief The key of the look-ahead horizon time property */
			static const std::string PROP_LOOK_AHEAD_TIME;
			
			/** @brief The key of the integrator step-size property */
			static const std::string PROP_LOOK_AHEAD_STEP_SIZE;

			/** @brief The key of the integrator step-size property */
			static const std::string PROP_INTEGRATOR_STEP_SIZE;

			/** @brief The key of the output channel property */
			static const std::string PROP_OUT;

			/** @brief The key of the input channel property */
			static const std::string PROP_IN;

			/**
			 * @brief Default C'tor initializing an empty application context object
			 */
			ApplicationContext(void);

			/**
			 * @brief Frees allocated resources
			 */
			~ApplicationContext(void);

			/**
			 * @brief Parses the command line argument list and appends the
			 * information.
			 * @details If the given argument vector is invalid a
			 * std::invalid_argument will be thrown. Each argument must have a
			 * key=value format. Each given key must be unique.
			 * @param argc The number of arguments in the argument vector
			 * @param argv The argument vector
			 */
			void addCommandlineProperties(int argc, const char *argv[]);

			/**
			 * @brief Generates sensitive default values based on the model 
			 * description and adds them.
			 * @details Previously set properties are not overwritten. The function
			 * assumes that the given reference is valid. If some previously set 
			 * property is invalid, Base::SystemConfigurationException will be thrown.
			 * @param description A reference to the model's static description
			 */
			void addSensitiveDefaultProperties(const ModelDescription * description);

			/**
			 * @brief Returns a pointer to the global output Base::ChannelMapping object
			 * @details The first invocation of the function will create the object. 
			 * Subsequent configuration changes may not be reflected by the output 
			 * channel mapping object. The function may throw a 
			 * SystemConfigurationException if some properties are missing. In this 
			 * case no object is generated.
			 */
			const ChannelMapping * getOutputChannelMapping(void);

			/**
			 * @brief Returns a pointer to the global input Base::ChannelMapping object
			 * @details The first invocation of the function will create the object.
			 * Subsequent configuration changes may not be reflected by the input
			 * channel mapping object. The function may throw a
			 * SystemConfigurationException if some properties are missing. In this
			 * case no object is generated.
			 */
			const ChannelMapping * getInputChannelMapping(void);

			/**
			 * @brief Returns a human readable string representation
			 * @details The function will not construct a channel mapping. In case 
			 * the channel mapping was not constructed beforehand, it will not be 
			 * included in the output.
			 */
			std::string toString() const;

		protected:
			/** @copydoc AbstractConfigProvider::getConfig() */
			virtual const boost::property_tree::ptree& getConfig() const 
			{ 
				return config_;
			}

		private:

			/** @brief Size of the internal error message buffers */
			static const int ERR_MSG_SIZE = 256;

			/**
			 * @brief The global configuration which stored the application's
			 * parameters
			 * @details The tree has to be populated by loading the program's 
			 * configuration sources such as CMD arguments or sensitive default
			 * values.
			 */
			boost::property_tree::ptree config_;

			/**
			 * @brief The globally unique source of PortIDs.
			 * @details The object is used to create unique PortIDs across multiple
			 * channel mapping objects.
			 */
			PortIDDrawer portIDSource_;

			/**
			 * @brief Pointer to the output channel mapping configuration
			 * @details The object will be created by the first query using
			 * getOutputChannelMapping().
			 */
			ChannelMapping * outputChannelMap_;

			/**
			 * @brief Pointer to the input channel mapping configuration.
			 * @details The object will be created by the first query using
			 * getInputChannelMapping().
			 */
			ChannelMapping * inputChannelMap_;

			/**
			 * @brief Extracts the key-value pair and adds it to the global
			 * configuration
			 * @details It is expected that the key is not empty and that the two
			 * parts are separated by a = sign. If the given option is invalid,
			 * std::invalid_argument will be thrown.
			 * @param opt A reference to the option string
			 * @param i The option's index used to generate meaningful error messages
			 */
			void addCommandlineOption(std::string &opt, int i);

			/**
			 * @brief Returns a newly created channel mapping object.
			 * @details The object must be deleted outside the function. If 
			 * the ChannelMapping cannot be created, a 
			 * Base::SystemConfigurationException will be thrown.
			 * @param propertyPrefix The prefix of the channels inside the property 
			 * tree. The sting is not stored and may be freed after the function 
			 * returns.
			 */
			ChannelMapping * newChannelMapping(const std::string &propertyPrefix);

		};

		/**
		 * @brief Prints the content of the ApplicationContext
		 */
		// Must be declared in the namespace of the argument! 
		// -> Otherwise some boost macros cannot resolve it
		std::ostream& operator<<(std::ostream& stream, 
			const ApplicationContext& appContext);
	}
}

#endif
