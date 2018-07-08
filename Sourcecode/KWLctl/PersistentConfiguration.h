/*
 * Copyright (C) 2018 Ivan Schréter (schreter@gmx.net)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * This copyright notice MUST APPEAR in all copies of the software!
 */

/*!
 * @file
 * @brief Helper to define a persistent configuration in EEPROM easily.
 */
#pragma once

class Print;

/*!
 * @brief Base class for PersistentConfiguration.
 *
 * @see ::PersistentConfiguration for documentation.
 */
class PersistentConfigurationBase
{
public:
  /*!
   * @brief Perform "factory reset".
   *
   * After the restart, EEPROM will be reprogrammed with default values.
   */
  void factoryReset();

protected:
  /// Function to load defaults.
  using LoadFnc = void (PersistentConfigurationBase::*)();

  PersistentConfigurationBase() {}

  /*!
   * @brief Load and validate the configuration.
   *
   * @param out stream where to print debug messages.
   * @param size configuration class size.
   * @param version configuration version.
   * @param load_defaults reference to loadDefaults() of the actual config.
   * @param reset if set, unconditionally reset EEPROM contents and load it with defaults.
   */
  void begin(Print& out, unsigned int size, unsigned int version, LoadFnc load_defaults, bool reset = false);

  /*!
   * @brief Update a member in the EEPROM.
   *
   * @param member member of the configuration to update.
   * @return @c true, if member updated, @c false if out of range.
   */
  template<typename T>
  bool update(const T& member) const {
    return updateRange(&member, sizeof(T));
  }

  /*!
   * @brief Update arbitrary section of the configuration.
   *
   * @param ptr pointer to part of the configuration.
   * @param size size to update.
   * @return @c true, if data updated, @c false if out of range.
   */
  bool updateRange(const void* ptr, unsigned int size) const;

  /*!
   * @brief Dump raw contents of the EEPROM to the specified stream.
   *
   * @param out stream to which to print.
   * @param bytes_per_row how many bytes to print per row.
   */
  static void dumpRaw(Print& out, unsigned bytes_per_row = 16);

private:
  /// Version counter.
  unsigned int version_;

  // NOTE: no data members allowed here (except version)!
};

/*!
 * @brief Helper to define a persistent configuration in EEPROM easily.
 *
 * Simply define your own class deriving from this configuration and
 * add members you'd like to store in EEPROM. Initially, call begin()
 * to load the configuration. Later, after updating a member, call
 * update() method to update it in EEPROM as well.
 *
 * It is also possible to update the entire configuration using
 * updateAll() call or an arbitrary part using updateRange().
 *
 * The class T must provide following:
 *    - void loadDefaults() - a method to initialize all variables if EEPROM
 *      doesn't contain the right version (as specified by Version),
 *    - data members (ideally private with accessors).
 *
 * If the configuration layout changes, the version should be incremented
 * to prevent using invalid data at runtime.
 *
 * Example:
 * @code
 * class MyConfig : public PersistentConfiguration<MyConfig>
 * {
 * public:
 *   int getMyValue() { return my_value_; }
 *   void setMyValue(int newValue) { my_value_ = newValue; update(value_); }
 * private:
 *   int my_value_;
 * };
 * @endcode
 */
template<typename T, unsigned int Version>
class PersistentConfiguration : public PersistentConfigurationBase
{
public:
  /*!
   * @brief Load the configuration.
   *
   * @param out stream for printing debugging messages.
   * @param reset if set, unconditionally reset EEPROM contents and load it with defaults.
   */
  void begin(Print& out, bool reset = false) {
    PersistentConfigurationBase::begin(out, sizeof(T), Version, static_cast<LoadFnc>(&T::loadDefaults), reset);
  }

  /*!
   * @brief Update entire configuration by writing it to EEPROM (SLOW).
   *
   * @return @c true, if data updated, @c false if out of range (too big).
   */
  bool updateAll() { return updateRange(this, sizeof(T)); }

  // NOTE: no data members allowed here!
};
