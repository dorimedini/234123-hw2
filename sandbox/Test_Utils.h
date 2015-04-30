/*
 * mtmtest.h
 *
 *  Created on: 03/06/2010
 *      Author: Strulovich
 */

#ifndef MTMTEST_H_
#define MTMTEST_H_

#include <iostream>
#include <sstream>
#include <string>
extern std::string location; // Used for finding the origin of an exception

#define FAIL(message) do { \
                std::ostringstream os; \
                os << __LINE__; \
                location = std::string(__FILE__) + ":" + os.str() + " " + message; \
                return false; \
        } while (0)

#define ASSERT_TRUE(expression) do { \
                if(!((expression))) { \
                		std::ostringstream s;	\
                		s << #expression << " is false";\
                        FAIL(s.str()); \
                } \
        } while (0)

#define ASSERT_FALSE(expression) do { \
                if((expression)) { \
                        FAIL("expression is false"); \
                } \
        } while (0)

#define ASSERT_EQUALS(expected,actual) do {     \
                if(!((expected) == (actual))) { \
                        FAIL("values not equal"); \
                } \
        } while (0)

#define ASSERT_NOT_EQUALS(expected,actual) do { \
                if((expected) == (actual)) { \
                        FAIL("values not equal"); \
                } \
        } while (0)

#define EPSILON 1e-10
#define ASSERT_DOUBLE_EQUALS(expected, actual) do { \
                if(!(((expected) + EPSILON >= actual) && ((expected) - EPSILON <= actual ))) { \
                        FAIL("expression not true"); \
                } \
        } while (0)

#define ASSERT_NO_THROW(command) do {\
                try { \
                        (command); \
                } catch (std::exception& e) { \
                        FAIL("caught "+e.what()); \
                } catch (...) { \
                        FAIL("caught unknown object"); \
                } \
        } while (0)

#define ASSERT_THROWS(exception_type,command) do {\
                try { \
                        (command); \
                        FAIL("missing exception"); \
                } catch (exception_type&) { \
                        \
                } catch (std::exception& e) { \
                        FAIL("caught "+e.what()); \
                } catch (...) { \
                        FAIL("caught unknown object"); \
                } \
        } while (0)
#define RUN_TEST(b) do {\
                try { \
                        std::cerr << #b << ":"; \
                        if (b()) std::cerr << "OK" << std::endl; \
                        else std::cerr << "Failed (" << location << ")" << std::endl; \
                } catch (...) { \
                        std::cerr << "Failed (" << location << ")" << std::endl; \
                } \
        } while (0)
#define RUN_SETUP(c) do {\
                try { \
                        if (!c) return false; \
                } catch (...) { \
                        return false; \
                } \
        } while (0)

#endif /* MTMTEST_H_ */
