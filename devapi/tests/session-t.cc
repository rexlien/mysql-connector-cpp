﻿/*
* Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
*
* The MySQL Connector/C++ is licensed under the terms of the GPLv2
* <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>, like most
* MySQL Connectors. There are special exceptions to the terms and
* conditions of the GPLv2 as it is applied to this software, see the
* FLOSS License Exception
* <http://www.mysql.com/about/legal/licensing/foss-exception.html>.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published
* by the Free Software Foundation; version 2 of the License.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
* for more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <test.h>
#include <iostream>


using std::cout;
using std::endl;
using namespace mysqlx;

class Sess : public mysqlx::test::Xplugin
{
};



TEST_F(Sess, databaseObj)
{
  SKIP_IF_NO_XPLUGIN;

  cout << "DatabaseObject implementation..." << endl;

  const string schema_name = "schemaObj";
  const string coll_name = "collObj";
  const string tbl_name = "tblObj";

  try {
    get_sess().dropSchema(schema_name);
  } catch (...) {}


  Schema schema = get_sess().createSchema(schema_name);

  //Test Schema Obj

  EXPECT_TRUE(schema.existsInDatabase());
  EXPECT_EQ(schema_name ,schema.getName());
  // EXPECT_FALSE(schema.getSession().getSchema("NOT_FOUND").existsInDatabase());

  //Test Collection Obj

  Collection coll = schema.createCollection(coll_name);

  EXPECT_TRUE(coll.existsInDatabase());
  EXPECT_EQ(coll_name, coll.getName());
  EXPECT_FALSE(coll.getSession().getSchema(schema_name)
               .getCollection("NOT_FOUND").existsInDatabase());


  //Test Table Obj

  {
    std::stringstream create_table;

    create_table << "CREATE TABLE"
      << "`" << schema_name << "`"
      << ".`" << tbl_name << "`"
      << "(name VARCHAR(20) ,age INT)" << std::ends;

    get_sess().sql(create_table.str()).execute();
  }

  Table tbl = schema.getTable(tbl_name);

  EXPECT_TRUE(tbl.existsInDatabase());
  EXPECT_EQ(tbl_name, tbl.getName());
  EXPECT_FALSE(tbl.getSession().getSchema(schema_name)
               .getTable("NOT_FOUND").existsInDatabase());


  cout << "Done!" << endl;
}


TEST_F(Sess, url)
{
  SKIP_IF_NO_XPLUGIN;

  using std::ostringstream;
  using std::ends;

  ostringstream buf;

  std::string user;

  if (m_user)
  {
    user = m_user;
    if (m_password)
      user = user + ":" + m_password;
    user = user + "@";
  }

  std::string authority = "localhost";
  if (m_port)
    authority = authority + ":" + std::to_string(m_port);

  std::string url = user + authority;

  {
    cout << "Creating session: " << url << endl;

    NodeSession sess(url);
    SqlResult res = sess.sql("SELECT @@version").execute();
    cout << "Talking to MySQL Server: " << res.fetchOne()[0] << endl;
  }

  {
    url = std::string("mysqlx://") + url;
    cout << "Creating session: " << url << endl;

    NodeSession sess(url);
    SqlResult res = sess.sql("SELECT @@version").execute();
    cout << "Talking to MySQL Server: " << res.fetchOne()[0] << endl;
  }

  // Test URL containing non-ascii characters.

  {
    mysqlx::string wurl = L"Rafa\u0142";
    wurl = wurl + string("@") + string(authority);

    cout << "Creating session: " << wurl << endl;

    try {
      NodeSession sess(wurl);
      SqlResult res = sess.sql("SELECT @@version").execute();
      cout << "Talking to MySQL Server: " << res.fetchOne()[0] << endl;
    }
    catch (const Error &e)
    {
      cout << "Expected error: " << e << endl;
    }
  }

  cout << "Done!" << endl;
}


TEST_F(Sess, default_schema)
{
  SKIP_IF_NO_XPLUGIN;

  EXPECT_THROW(get_sess().getDefaultSchema(),Error);

  {
    mysqlx::NodeSession s(m_port, m_user, m_password, "test");

    EXPECT_EQ(string("test"), s.getDefaultSchema().getName());
    EXPECT_EQ(string("test"), s.getDefaultSchemaName());
    SqlResult res = s.sql(L"SELECT DATABASE()").execute();
    string db = res.fetchOne()[0];
    EXPECT_EQ(string("test"), db);
  }

  {
    std::string url = m_user;
    url = url + "@localhost";
    if (m_port)
      url = url + ":" + std::to_string(m_port);
    url = url + "/test";

    mysqlx::NodeSession s(url);

    EXPECT_EQ(string("test"), s.getDefaultSchema().getName());
    EXPECT_EQ(string("test"), s.getDefaultSchemaName());
    SqlResult res = s.sql(L"SELECT DATABASE()").execute();
    string db = res.fetchOne()[0];
    EXPECT_EQ(string("test"), db);
  }

  cout << "Done!" << endl;
}


TEST_F(Sess, trx)
{
  SKIP_IF_NO_XPLUGIN;

  Collection coll = get_sess().getSchema("test").createCollection("c", true);
  coll.remove().execute();

  try {
    coll.getSession().startTransaction();
    coll.add("{\"foo\": 1}").execute();
    coll.add("{\"foo\": 2}").execute();
    coll.getSession().commit();

    coll.getSession().startTransaction();
    coll.add("{\"bar\": 3}").execute();
    coll.add("{\"bar\": 4}").execute();
    coll.getSession().rollback();
  }
  catch (...)
  {
    get_sess().rollback();
    throw;
  }

  cout << "After insertions collection has " << coll.count()
       << " documents." << endl;

  EXPECT_EQ(2U, coll.count());

  for (DbDoc doc : coll.find().execute())
  {
    cout << "- doc: " << doc << endl;
    EXPECT_FALSE(doc.hasField("bar"));
  }

  /*
    Check that deleted session rollbacks transaction that is not
    committed.
  */

  {
    XSession sess(this);
    Collection coll = sess.getSchema("test").getCollection("c");

    sess.startTransaction();
    coll.add("{\"bar\": 5}").execute();
    coll.add("{\"bar\": 6}").execute();
  }

  cout << "Collection has " << coll.count()
    << " documents." << endl;

  EXPECT_EQ(2U, coll.count());

  for (DbDoc doc : coll.find().execute())
  {
    cout << "- doc: " << doc << endl;
    EXPECT_FALSE(doc.hasField("bar"));
  }

  /*
    Check error thrown if starting new transaction while previous
    one is not closed.
  */

  get_sess().startTransaction();

  try {
    get_sess().startTransaction();
    FAIL() << "Expected an error";
  }
  catch (const Error &e)
  {
    cout << "Expected error: " << e << endl;
  }

  cout << "Done!" << endl;
}


TEST_F(Sess, bind_node_session)
{
  SKIP_IF_NO_XPLUGIN;

   XSession *sess = new XSession(this);

   sess->dropSchema("node_session");
   Schema sch = sess->createSchema("node_session");

   // Create Collection with data
   sch.createCollection("coll1")
       .add("{\"name\":\"foo\"}")
       .add("{\"name\":\"bar\"}")
       .add("{\"name\":\"baz\"}").execute();

   {
     // Get first NodeSession
     NodeSession node= sess->bindToDefaultShard();

     // Execute query to be checked later
     SqlResult res = node.sql("select * from node_session.coll1").execute();

     // Closing NodeSession
     // Only affects this session, no changes on other nodes or master XSession
     node.close();

     // Expect throw Error() because session is closed
     EXPECT_THROW(node.sql("select * from node_session.coll1")
                  .execute(),
                  mysqlx::Error);

     {
       // Get second NodeSession
       NodeSession node2= sess->bindToDefaultShard();

       // Execute query but doesn't retrieve results right away
       SqlResult res2 = node2.sql("select * from node_session.coll1").execute();

       //Close Session so other NodeSession obj are closed.
       sess->close();

       // Expect throw Error() because session is closed
       EXPECT_THROW(node2.sql("select * from node_session.coll1")
                                .execute(),
                    mysqlx::Error);


       // Delete Xsession, so other NodeSession obj are closed.
       delete sess;

       // Expect throw Error() because session is closed
       EXPECT_THROW(node2.sql("select * from node_session.coll1")
                                .execute(),
                    mysqlx::Error);

       // Results are available, because where cached when destructing XSession
       EXPECT_EQ(3U, res2.count());


       int i = 0;
       for (auto row : res2)
       {
         std::cout << "Row " << i << ": "
                    << static_cast<string>(row[1]) << std::endl;
         ++i;
       }

       EXPECT_EQ(3U, i);
     }

     //Same here
     EXPECT_EQ(3U, res.count());

     unsigned i = 0;
     for (auto row : res)
     {
       std::cout << "Row " << i << ": " << static_cast<string>(row[1]) << std::endl;
       ++i;
     }

     EXPECT_EQ(3U, i);
   }

  cout << "Done!" << endl;
}


TEST_F(Sess, ssl_session)
{

  SKIP_IF_NO_XPLUGIN;

  //Test if ssl is enabled using cipher
  auto check_ssl_impl = [](mysqlx::XSession &sess, bool enable, int line)
  {
    SqlResult res =  sess.bindToDefaultShard().sql("SHOW STATUS LIKE 'mysqlx_ssl_cipher'").execute();

    auto row = res.fetchOne();
    cout << "Line "<< line << ": " << row[0] << ":" << row[1] << endl;

    string cipher = row[1];

    EXPECT_EQ(enable, !cipher.empty());
  };

#define check_ssl(x,y) check_ssl_impl(x, y, __LINE__)


  {
    mysqlx::XSession sess(SessionSettings::PORT, get_port(),
                          SessionSettings::USER,get_user(),
                          SessionSettings::PWD, get_password() ? get_password() : nullptr
                          );

    check_ssl(sess, true);
  }

  {
    mysqlx::XSession sess(SessionSettings::PORT, get_port(),
                          SessionSettings::USER, get_user(),
                          SessionSettings::PWD, get_password() ? get_password() : nullptr,
                          SessionSettings::SSL_MODE, SessionSettings::SSLMode::DISABLED
                          );

    check_ssl(sess, false);
  }

  //Using URI

  std::stringstream uri;

  uri << "mysqlx://" << get_user();

  if (get_password() && *get_password())
    uri << ":"<< get_password();

  uri << "@" << "localhost:" << get_port();

  //URI using ssl-mode=disabled
  {
    std::stringstream ssl_off;
    ssl_off << uri.str() << "/?sSl-mODe=DIsabled";
    mysqlx::XSession sess(ssl_off.str());
    check_ssl(sess, false);
  }


  {
    std::stringstream uri_ssl;
    //Enable SSL
    uri_ssl << uri.str() << "/?SSl-Mode=RequireD";

    mysqlx::XSession sess(uri_ssl.str());
    check_ssl(sess, true);
  }

  {
    std::stringstream uri_wrong;
    uri_wrong << uri.str() << "&ssl-nonexisting=true";
    EXPECT_THROW(mysqlx::XSession sess(uri_wrong.str()), mysqlx::Error);
  }

  //using wrong ssl-ca as SessionSettings
  {
    EXPECT_THROW(
    mysqlx::XSession sess(SessionSettings::PORT, get_port(),
                          SessionSettings::USER,get_user(),
                          SessionSettings::PWD, get_password() ? get_password() : nullptr ,
                          SessionSettings::SSL_MODE, SessionSettings::SSLMode::VERIFY_CA,
                          SessionSettings::SSL_CA, "unknown")
          , mysqlx::Error);


  }

  //using wrong ssl-ca and ssl-ca-path on URI
  {
    std::stringstream bad_uri;
    bad_uri << uri.str() << "/?Ssl-modE=VErify_Ca&sSl-Ca=" << "unknown.file";

    EXPECT_THROW(mysqlx::XSession sess(bad_uri.str()), mysqlx::Error);
  }

  string ssl_ca;
  string datadir;

  {
    mysqlx::XSession sess(uri.str());

    SqlResult res = sess.bindToDefaultShard()
                    .sql("show global variables like 'ssl_ca'")
                    .execute();

    ssl_ca = res.fetchOne().get(1);

    res = sess.bindToDefaultShard()
          .sql("show global variables like 'datadir'")
          .execute();

    datadir = res.fetchOne().get(1);

  }

  std::cout << "ssl-ca:" << ssl_ca
             << " datadir:" << datadir
             << std::endl;

  if (ssl_ca.find('\\') == string::npos && ssl_ca.find('/') == string::npos)
  { //not full path
    ssl_ca = datadir + ssl_ca;
  }

  uri << "/?ssl-ca=" << ssl_ca;

  {
    mysqlx::XSession sess(uri.str());
    check_ssl(sess, true);
  }

  //using ssl-enable and ssl-ca as SessionSettings
  {
    mysqlx::XSession sess(SessionSettings::PORT, get_port(),
                          SessionSettings::USER,get_user(),
                          SessionSettings::PWD, get_password() ? get_password() : nullptr ,
                          SessionSettings::SSL_MODE, SessionSettings::SSLMode::VERIFY_CA,
                          SessionSettings::SSL_CA, ssl_ca);

    check_ssl(sess, true);

  }

  //using ssl-ca as SessionSettings
  {
    mysqlx::XSession sess(SessionSettings::PORT, get_port(),
                          SessionSettings::USER,get_user(),
                          SessionSettings::PWD, get_password() ? get_password() : nullptr ,
                          SessionSettings::SSL_CA, ssl_ca);

    check_ssl(sess, true);

  }

  //using ssl-ca but ssl-enable = false on SessionSettings
  {
    mysqlx::XSession sess(SessionSettings::PORT, get_port(),
                          SessionSettings::USER,get_user(),
                          SessionSettings::PWD, get_password() ? get_password() : nullptr ,
                          SessionSettings::SSL_MODE, SessionSettings::SSLMode::DISABLED,
                          SessionSettings::SSL_CA, ssl_ca);

    check_ssl(sess, false);

  }

  //using ssl-enable and ssl-ca as SessionSettings
  {

    try {
      mysqlx::XSession sess(SessionSettings::PORT, get_port(),
                            SessionSettings::USER,get_user(),
                            SessionSettings::PWD, get_password() ? get_password() : NULL ,
                            SessionSettings::SSL_MODE, SessionSettings::SSLMode::VERIFY_IDENTITY,
                            SessionSettings::SSL_CA, ssl_ca);

      // If server cert CN=localhost, verification will succeed, and ssl is
      // enabled
      check_ssl(sess, true);
    }
    catch (Error &e)
    {
      // If server cert CN!=localhost, it will fail with this error
      EXPECT_EQ(string("CDK Error: yaSSL: SSL certificate validation failure"),
                string(e.what()));
    }

  }

}


TEST_F(Sess, ipv6)
{

  SKIP_IF_NO_XPLUGIN;

  {
    mysqlx::XSession sess(SessionSettings::HOST, "::1",
                          SessionSettings::PORT, get_port(),
                          SessionSettings::USER, get_user(),
                          SessionSettings::PWD, get_password() ? get_password() : nullptr,
                          SessionSettings::SSL_MODE,SessionSettings::SSLMode::DISABLED
                          );
  }

  //Using URI

  std::stringstream uri;

  uri << "mysqlx://" << get_user();

  if (get_password() && *get_password())
    uri << ":"<< get_password();

  uri << "@" << "[::1]:" << get_port();

  //URI without ssl_mode
  {
    mysqlx::XSession sess(uri.str());

    SqlResult res =  sess.bindToDefaultShard().sql("SHOW STATUS LIKE 'mysqlx_ssl_cipher'").execute();

    auto row = res.fetchOne();
    cout << row[0] << ":" << row[1] << endl;

    string cipher = row[1];

    EXPECT_FALSE(cipher.empty());
  }

  //Disable SSL_MODE
  uri << "/?Ssl-Mode=DisabLED";
  {
    mysqlx::XSession sess(uri.str());

    SqlResult res =  sess.bindToDefaultShard().sql("SHOW STATUS LIKE 'mysqlx_ssl_cipher'").execute();

    auto row = res.fetchOne();
    cout << row[0] << ":" << row[1] << endl;

    string cipher = row[1];

    EXPECT_TRUE(cipher.empty());
  }
}

TEST_F(Sess, failover)
{

  {
    XSession s(this);

    s.createSchema("test", true);
  }

  //URI multiple hosts tests
  {
    std::stringstream uri;

    uri << "mysqlx://" << get_user();

    if (get_password())
      uri << ":" << get_password();

    uri << "@["
           "localhost6,"
           "127.0.1.250:33060,"
           "[::2]:1,";
    uri << "127.0.0.1";
    if (get_port() != 0)
      uri << ":" <<get_port();

    uri << "]/test";

    mysqlx::XSession s(uri.str());

    EXPECT_EQ(string("test"),s.getDefaultSchema().getName());
  }

  //URI multiple hosts tests with priority
  {
    std::stringstream uri;

    uri << "mysqlx://" << get_user();

    if (get_password())
      uri << ":" << get_password();

    uri << "@["
           "(address=localhost6, priority=99) ,"
           "(address=127.0.1.250:33060, priority=99),"
           "(address=[::2]:1, priority=1),";
    uri << "(address=127.0.0.1";
    if (get_port() != 0)
      uri << ":" <<get_port();
    uri << ", priority=100)";

    uri << "]/test";

    mysqlx::XSession s(uri.str());

    EXPECT_EQ(string("test"),s.getDefaultSchema().getName());
  }

  {
    mysqlx::XSession s(SessionSettings::USER, get_user(),
                       SessionSettings::PWD, get_password() ? get_password() : nullptr,
                       SessionSettings::HOST, "server.example.com",
                       SessionSettings::PRIORITY, 1,
                       SessionSettings::HOST, "192.0.2.11",
                       SessionSettings::PORT, 33060,
                       SessionSettings::PRIORITY, 99,
                       SessionSettings::HOST, "[2001:db8:85a3:8d3:1319:8a2e:370:7348]",
                       SessionSettings::PORT, 1,
                       SessionSettings::PRIORITY, 2,
                       SessionSettings::HOST, "::1",
                       SessionSettings::PORT, get_port(),
                       SessionSettings::PRIORITY, 100,
                       SessionSettings::DB, "test");

    EXPECT_EQ(string("test"),s.getDefaultSchema().getName());
  }

  //SessionSettings::set() tests
  {
    SessionSettings settings(SessionSettings::USER, get_user(),
                             SessionSettings::PWD, get_password() ?
                               get_password() :
                               nullptr);

    EXPECT_THROW(settings.set(SessionSettings::PORT, get_port()), Error);

    EXPECT_THROW(settings.set(SessionSettings::PRIORITY, 1), Error);

    EXPECT_THROW(settings.set(SessionSettings::HOST, "server.example.com",
                              SessionSettings::USER, get_user(),
                              SessionSettings::PORT, 1,
                              SessionSettings::PRIORITY, 1), Error);

    settings.erase(SessionSettings::HOST);

    settings.set(SessionSettings::HOST, "server.example.com",
                 SessionSettings::PRIORITY, 1,
                 SessionSettings::USER, get_user(),
                 SessionSettings::PWD, get_password() ?
                   get_password() :
                   nullptr,
                 SessionSettings::HOST, "192.0.2.11",
                 SessionSettings::PORT, 33060,
                 SessionSettings::PRIORITY, 98,
                 SessionSettings::HOST, "[2001:db8:85a3:8d3:1319:8a2e:370:7348]",
                 SessionSettings::PRIORITY, 2,
                 SessionSettings::HOST, "::1",
                 SessionSettings::PORT, get_port(),
                 SessionSettings::PRIORITY, 99,
                 SessionSettings::DB, "test"
                 );


    mysqlx::XSession s(settings);

    EXPECT_EQ(string("test"),s.getDefaultSchema().getName());
  }

  //SessionSettings::set() tests without Port and Priority
  {
    SessionSettings settings(SessionSettings::USER, get_user(),
                             SessionSettings::PWD, get_password() ?
                               get_password() :
                               nullptr);

    settings.set(SessionSettings::HOST, "192.0.2.11",
                 SessionSettings::DB, "test"
                 );


    EXPECT_THROW(mysqlx::XSession s(settings), Error);
  }

  //Multiple host with 1st host defined only by port
  {
    SessionSettings settings(SessionSettings::USER, get_user(),
                             SessionSettings::PWD, get_password() ?
                               get_password() :
                               nullptr,
                             SessionSettings::PORT, 13009);

    settings.set(SessionSettings::HOST, "192.0.2.11",
                 SessionSettings::PORT, 33060);

    EXPECT_THROW(mysqlx::XSession s(settings), Error);
  }


}


TEST_F(Sess, bugs)
{

  {
    SessionSettings sess_settings("localhost_not_found", 13009, "rafal", (char*)NULL);

    EXPECT_THROW(mysqlx::XSession(sess_settings), mysqlx::Error);
  }

  {
    SessionSettings sess_settings("localhost_not_found", 13009, "rafal", NULL);

    EXPECT_THROW(mysqlx::XSession(sess_settings), mysqlx::Error);
  }

}