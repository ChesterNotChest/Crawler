#include "sqlinterface.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDebug>
#include <QThread>

SQLInterface::SQLInterface() {}
SQLInterface::~SQLInterface() { disconnect(); }

bool SQLInterface::openSqliteConnection(const QString &dbFilePath) {

	// store DB path for lazy per-thread connection creation
	m_dbFilePath = dbFilePath;
	// create a connection for the current thread as an initial connection
	const QString connName = QStringLiteral("crawler_conn_%1").arg((quintptr)QThread::currentThreadId());
	QSqlDatabase db;
	if (QSqlDatabase::contains(connName)) {
		db = QSqlDatabase::database(connName);
	} else {
		db = QSqlDatabase::addDatabase("QSQLITE", connName);
	}
	db.setDatabaseName(dbFilePath);
	if (!db.open()) {
		qDebug() << "SQLite open failed:" << db.lastError().text();
		return false;
	}
	return true;
}

bool SQLInterface::connectSqlite(const QString &dbFilePath) {
	return openSqliteConnection(dbFilePath);
}

bool SQLInterface::isConnected() const {
	// Ensure there is a DB file path configured
	if (m_dbFilePath.isEmpty()) return false;
	// Attempt to get or create a connection for this thread.
	// NOTE: databaseForCurrentThread() is non-const; create a temporary non-const pointer to call it.
	SQLInterface *self = const_cast<SQLInterface*>(this);
	QSqlDatabase db;
	try {
		db = self->databaseForCurrentThread();
	} catch (...) {
		return false;
	}
	return db.isValid() && db.isOpen();
}

void SQLInterface::disconnect() {
	// remove all crawler_conn_* connections
	const auto names = QSqlDatabase::connectionNames();
	for (const QString &name : names) {
		if (name.startsWith("crawler_conn_")) {
			if (QSqlDatabase::contains(name)) {
				QSqlDatabase db = QSqlDatabase::database(name);
				if (db.isOpen()) db.close();
			}
			QSqlDatabase::removeDatabase(name);
		}
	}
}

bool SQLInterface::createAllTables() {
	if (!isConnected()) {
		qDebug() << "Not connected; cannot create tables.";
		return false;
	}
	QSqlDatabase db = databaseForCurrentThread();

	// Source (数据来源表)
	QSqlQuery q(db);
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS Source ("
		" sourceId INTEGER PRIMARY KEY AUTOINCREMENT,"
		" sourceName TEXT NOT NULL,"
		" sourceCode TEXT NOT NULL UNIQUE,"
		" baseUrl TEXT,"
		" enabled INTEGER DEFAULT 1,"
		" createdAt TEXT,"
		" updatedAt TEXT"
		")")) {
		qDebug() << "Create Source failed:" << q.lastError().text();
		return false;
	}

	// RecruitType (fixed enum: 1=校招, 2=实习, 3=社招)
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS RecruitType ("
		" recruitTypeId INTEGER PRIMARY KEY,"
		" typeName TEXT NOT NULL UNIQUE"
		")")) {
		qDebug() << "Create RecruitType failed:" << q.lastError().text();
		return false;
	}

	// Company
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS Company ("
		" companyId INTEGER PRIMARY KEY,"
		" companyName TEXT NOT NULL"
		")")) {
		qDebug() << "Create Company failed:" << q.lastError().text();
		return false;
	}

	// JobCity
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS JobCity ("
		" cityId INTEGER PRIMARY KEY AUTOINCREMENT,"
		" cityName TEXT NOT NULL UNIQUE"
		")")) {
		qDebug() << "Create JobCity failed:" << q.lastError().text();
		return false;
	}

	// JobTag
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS JobTag ("
		" tagId INTEGER PRIMARY KEY AUTOINCREMENT,"
		" tagName TEXT NOT NULL UNIQUE"
		")")) {
		qDebug() << "Create JobTag failed:" << q.lastError().text();
		return false;
	}


	// Job (主表)
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS Job ("
		" jobId INTEGER PRIMARY KEY,"
		" jobName TEXT NOT NULL,"
		" companyId INTEGER,"
		" recruitTypeId INTEGER,"
		" cityId INTEGER,"
		" sourceId INTEGER,"
		" requirements TEXT,"
		" salaryMin REAL,"
		" salaryMax REAL,"
		" salarySlabId INTEGER,"
		" createTime TEXT,"
		" updateTime TEXT,"
		" hrLastLoginTime TEXT,"
		" FOREIGN KEY(sourceId) REFERENCES Source(sourceId)"
		")")) {
		qDebug() << "Create Job failed:" << q.lastError().text();
		return false;
	}

	// JobTagMapping (many-to-many)
	if (!q.exec(
		"CREATE TABLE IF NOT EXISTS JobTagMapping ("
		" jobId INTEGER,"
		" tagId INTEGER,"
		" PRIMARY KEY (jobId, tagId)"
		")")) {
		qDebug() << "Create JobTagMapping failed:" << q.lastError().text();
		return false;
	}

	// Database migration: Add sourceId column to Job table if not exists
	if (q.exec("PRAGMA table_info(Job)")) {
		bool hasSourceId = false;
		while (q.next()) {
			QString columnName = q.value(1).toString();
			if (columnName == "sourceId") {
				hasSourceId = true;
				break;
			}
		}
		
		if (!hasSourceId) {
			qDebug() << "[Migration] Adding sourceId column to Job table...";
			if (!q.exec("ALTER TABLE Job ADD COLUMN sourceId INTEGER")) {
				qDebug() << "Failed to add sourceId column:" << q.lastError().text();
				return false;
			}
			qDebug() << "[Migration] sourceId column added successfully.";
		}
	}

	// Initialize RecruitType enum values if empty
	if (!q.exec("SELECT COUNT(*) FROM RecruitType")) {
		qDebug() << "Count RecruitType failed:" << q.lastError().text();
		return false;
	}
	q.next();
	if (q.value(0).toInt() == 0) {
		q.prepare("INSERT INTO RecruitType(recruitTypeId, typeName) VALUES(:id, :name)");
		const QVector<QPair<int, QString>> types{{1, "校招"}, {2, "实习"}, {3, "社招"}};
		for (const auto &t : types) {
			q.bindValue(":id", t.first);
			q.bindValue(":name", t.second);
			if (!q.exec()) {
				qDebug() << "Insert RecruitType failed:" << q.lastError().text();
				return false;
			}
		}
	}

	// Initialize Source table with default sources if empty
	if (!q.exec("SELECT COUNT(*) FROM Source")) {
		qDebug() << "Count Source failed:" << q.lastError().text();
		return false;
	}
	q.next();
	if (q.value(0).toInt() == 0) {
		q.prepare("INSERT INTO Source(sourceName, sourceCode, baseUrl, enabled, createdAt, updatedAt) "
				  "VALUES(:name, :code, :url, :enabled, datetime('now'), datetime('now'))");
		const QVector<QPair<QString, QPair<QString, QString>>> sources{
			{"牛客网", {"nowcode", "https://www.nowcoder.com"}},
			{"BOSS直聘", {"zhipin", "https://www.zhipin.com"}}
		};
		for (const auto &s : sources) {
			q.bindValue(":name", s.first);
			q.bindValue(":code", s.second.first);
			q.bindValue(":url", s.second.second);
			q.bindValue(":enabled", 1);
			if (!q.exec()) {
				qDebug() << "Insert Source failed:" << q.lastError().text();
				return false;
			}
		}
	}

	qDebug() << "All tables created successfully.";
	return true;
}

// Source operations
int SQLInterface::insertSource(const SQLNS::Source &source) {
	if (!isConnected()) return -1;
	QSqlDatabase db = databaseForCurrentThread();
	QSqlQuery q(db);
	q.prepare("INSERT OR IGNORE INTO Source(sourceName, sourceCode, baseUrl, enabled, createdAt, updatedAt) "
			  "VALUES(:name, :code, :url, :enabled, datetime('now'), datetime('now'))");
	q.bindValue(":name", source.sourceName);
	q.bindValue(":code", source.sourceCode);
	q.bindValue(":url", source.baseUrl);
	q.bindValue(":enabled", source.enabled ? 1 : 0);
	q.exec(); // ignore duplicates
	q.prepare("SELECT sourceId FROM Source WHERE sourceCode = :code");
	q.bindValue(":code", source.sourceCode);
	if (q.exec() && q.next()) {
		return q.value(0).toInt();
	}
	return -1;
}

SQLNS::Source SQLInterface::querySourceById(int sourceId) {
	SQLNS::Source source;
	source.sourceId = -1;
	if (!isConnected()) return source;
	QSqlDatabase db = databaseForCurrentThread();
	QSqlQuery q(db);
	q.prepare("SELECT sourceId, sourceName, sourceCode, baseUrl, enabled, createdAt, updatedAt "
			  "FROM Source WHERE sourceId = :id");
	q.bindValue(":id", sourceId);
	if (q.exec() && q.next()) {
		source.sourceId = q.value(0).toInt();
		source.sourceName = q.value(1).toString();
		source.sourceCode = q.value(2).toString();
		source.baseUrl = q.value(3).toString();
		source.enabled = q.value(4).toInt() != 0;
		source.createdAt = q.value(5).toString();
		source.updatedAt = q.value(6).toString();
	}
	return source;
}

SQLNS::Source SQLInterface::querySourceByCode(const QString &sourceCode) {
	SQLNS::Source source;
	source.sourceId = -1;
	if (!isConnected()) return source;
	QSqlDatabase db = databaseForCurrentThread();
	QSqlQuery q(db);
	q.prepare("SELECT sourceId, sourceName, sourceCode, baseUrl, enabled, createdAt, updatedAt "
			  "FROM Source WHERE sourceCode = :code");
	q.bindValue(":code", sourceCode);
	if (q.exec() && q.next()) {
		source.sourceId = q.value(0).toInt();
		source.sourceName = q.value(1).toString();
		source.sourceCode = q.value(2).toString();
		source.baseUrl = q.value(3).toString();
		source.enabled = q.value(4).toInt() != 0;
		source.createdAt = q.value(5).toString();
		source.updatedAt = q.value(6).toString();
	}
	return source;
}

QVector<SQLNS::Source> SQLInterface::queryAllSources() {
	QVector<SQLNS::Source> sources;
	if (!isConnected()) return sources;
	QSqlDatabase db = databaseForCurrentThread();
	QSqlQuery q(db);
	if (q.exec("SELECT sourceId, sourceName, sourceCode, baseUrl, enabled, createdAt, updatedAt FROM Source")) {
		while (q.next()) {
			SQLNS::Source source;
			source.sourceId = q.value(0).toInt();
			source.sourceName = q.value(1).toString();
			source.sourceCode = q.value(2).toString();
			source.baseUrl = q.value(3).toString();
			source.enabled = q.value(4).toInt() != 0;
			source.createdAt = q.value(5).toString();
			source.updatedAt = q.value(6).toString();
			sources.append(source);
		}
	}
	return sources;
}

QVector<SQLNS::Source> SQLInterface::queryEnabledSources() {
	QVector<SQLNS::Source> sources;
	if (!isConnected()) return sources;
	QSqlDatabase db = databaseForCurrentThread();
	QSqlQuery q(db);
	if (q.exec("SELECT sourceId, sourceName, sourceCode, baseUrl, enabled, createdAt, updatedAt "
			   "FROM Source WHERE enabled = 1")) {
		while (q.next()) {
			SQLNS::Source source;
			source.sourceId = q.value(0).toInt();
			source.sourceName = q.value(1).toString();
			source.sourceCode = q.value(2).toString();
			source.baseUrl = q.value(3).toString();
			source.enabled = q.value(4).toInt() != 0;
			source.createdAt = q.value(5).toString();
			source.updatedAt = q.value(6).toString();
			sources.append(source);
		}
	}
	return sources;
}

int SQLInterface::insertCompany(int companyId, const QString &companyName) {
	if (!isConnected()) return -1;
	QSqlDatabase db = databaseForCurrentThread();

	// Insert or ignore if the company already exists (avoid UNIQUE constraint errors)
	{
		QSqlQuery q(db);
		q.prepare("INSERT OR IGNORE INTO Company(companyId, companyName) VALUES(:id, :name)");
		q.bindValue(":id", companyId);
		q.bindValue(":name", companyName);
		q.exec(); // ignore duplicates silently
	}

	// Optional: update companyName if it differs and non-empty
	if (!companyName.isEmpty()) {
		QSqlQuery qs(db);
		qs.prepare("SELECT companyName FROM Company WHERE companyId = :id");
		qs.bindValue(":id", companyId);
		if (qs.exec() && qs.next()) {
			const QString existingName = qs.value(0).toString();
			if (existingName != companyName) {
				QSqlQuery qu(db);
				qu.prepare("UPDATE Company SET companyName = :name WHERE companyId = :id");
				qu.bindValue(":name", companyName);
				qu.bindValue(":id", companyId);
				qu.exec();
			}
			return companyId;
		}
	}

	// As a fallback, verify the row exists
	{
		QSqlQuery qv(db);
		qv.prepare("SELECT companyId FROM Company WHERE companyId = :id");
		qv.bindValue(":id", companyId);
		if (qv.exec() && qv.next()) {
			return companyId;
		}
	}

	return -1;
}

int SQLInterface::insertCity(const QString &cityName) {
	if (!isConnected()) return -1;
	QSqlDatabase db = databaseForCurrentThread();
	QSqlQuery q(db);
	q.prepare("INSERT OR IGNORE INTO JobCity(cityName) VALUES(:name)");
	q.bindValue(":name", cityName);
	q.exec(); // ignore duplicates
	q.prepare("SELECT cityId FROM JobCity WHERE cityName = :name");
	q.bindValue(":name", cityName);
	if (q.exec() && q.next()) {
		return q.value(0).toInt();
	}
	return -1;
}

int SQLInterface::insertTag(const QString &tagName) {
	if (!isConnected()) return -1;
	QSqlDatabase db = databaseForCurrentThread();
	QSqlQuery q(db);
	q.prepare("INSERT OR IGNORE INTO JobTag(tagName) VALUES(:name)");
	q.bindValue(":name", tagName);
	q.exec(); // ignore duplicates
	q.prepare("SELECT tagId FROM JobTag WHERE tagName = :name");
	q.bindValue(":name", tagName);
	if (q.exec() && q.next()) {
		return q.value(0).toInt();
	}
	return -1;
}

int SQLInterface::insertJob(const SQLNS::JobInfo &job) {
	if (!isConnected()) return -1;
	QSqlDatabase db = databaseForCurrentThread();
	{
		QSqlQuery q(db);
		q.prepare(
			"INSERT OR IGNORE INTO Job(jobId, jobName, companyId, recruitTypeId, cityId, sourceId, "
			"requirements, salaryMin, salaryMax, salarySlabId, createTime, updateTime, hrLastLoginTime) "
			"VALUES(:jobId, :jobName, :companyId, :recruitTypeId, :cityId, :sourceId, "
			":requirements, :salaryMin, :salaryMax, :salarySlabId, :createTime, :updateTime, :hrLastLoginTime)");
		q.bindValue(":jobId", QVariant::fromValue<qlonglong>(job.jobId));
		q.bindValue(":jobName", job.jobName);
		q.bindValue(":companyId", job.companyId);
		q.bindValue(":recruitTypeId", job.recruitTypeId);
		q.bindValue(":cityId", job.cityId);
		q.bindValue(":sourceId", job.sourceId);
		q.bindValue(":requirements", job.requirements);
		q.bindValue(":salaryMin", job.salaryMin);
		q.bindValue(":salaryMax", job.salaryMax);
		q.bindValue(":salarySlabId", job.salarySlabId);
		q.bindValue(":createTime", job.createTime);
		q.bindValue(":updateTime", job.updateTime);
		q.bindValue(":hrLastLoginTime", job.hrLastLoginTime);
		if (!q.exec()) {
			qDebug() << "Insert Job failed:" << q.lastError().text();
			qDebug() << "Query:" << q.lastQuery();
			qDebug() << "DB connection:" << db.connectionName() << " valid:" << db.isValid() << " open:" << db.isOpen();
			qDebug() << "Job debug -> jobId:" << job.jobId << " jobName:" << job.jobName << " companyId:" << job.companyId << " sourceId:" << job.sourceId;
			return -1;
		}
	}

	// Ensure the row exists (either newly inserted or pre-existing)
	{
		QSqlQuery q2(db);
		q2.prepare("SELECT jobId FROM Job WHERE jobId = :jobId");
		q2.bindValue(":jobId", QVariant::fromValue<qlonglong>(job.jobId));
		if (q2.exec() && q2.next()) {
			return static_cast<int>(job.jobId);
		} else {
			qDebug() << "Insert Job: verify select failed:" << q2.lastError().text();
			return -1;
		}
	}
}

bool SQLInterface::insertJobTagMapping(long long jobId, int tagId) {
	if (!isConnected()) return false;
	QSqlDatabase db = databaseForCurrentThread();
	QSqlQuery q(db);
	q.prepare("INSERT OR IGNORE INTO JobTagMapping(jobId, tagId) VALUES(:jobId, :tagId)");
	q.bindValue(":jobId", QVariant::fromValue<qlonglong>(jobId));
	q.bindValue(":tagId", tagId);
	return q.exec();
}

QVector<SQLNS::JobInfo> SQLInterface::queryAllJobs() {
	QVector<SQLNS::JobInfo> jobs;
	if (!isConnected()) return jobs;
	QSqlDatabase db = databaseForCurrentThread();
	QSqlQuery q(db);
	if (!q.exec("SELECT jobId, jobName, companyId, recruitTypeId, cityId, sourceId, requirements, "
				"salaryMin, salaryMax, salarySlabId, createTime, updateTime, hrLastLoginTime "
				"FROM Job ORDER BY jobId ASC")) {
		qDebug() << "Select Job failed:" << q.lastError().text();
		return jobs;
	}
	while (q.next()) {
		SQLNS::JobInfo job;
		job.jobId = q.value(0).toLongLong();
		job.jobName = q.value(1).toString();
		job.companyId = q.value(2).toInt();
		job.recruitTypeId = q.value(3).toInt();
		job.cityId = q.value(4).toInt();
		job.sourceId = q.value(5).toInt();
		job.requirements = q.value(6).toString();
		job.salaryMin = q.value(7).toDouble();
		job.salaryMax = q.value(8).toDouble();
		job.salarySlabId = q.value(9).toInt();
		job.createTime = q.value(10).toString();
		job.updateTime = q.value(11).toString();
		job.hrLastLoginTime = q.value(12).toString();

		// Query tags for this job
		QSqlQuery tagQuery(db);
		tagQuery.prepare("SELECT t.tagId FROM JobTag t "
						"JOIN JobTagMapping m ON t.tagId = m.tagId "
						"WHERE m.jobId = :jobId");
		tagQuery.bindValue(":jobId", job.jobId);
		if (tagQuery.exec()) {
			while (tagQuery.next()) {
				job.tagIds.append(tagQuery.value(0).toInt());
			}
		}

		jobs.append(job);
	}

	
	return jobs;
}

QVector<SQLNS::JobInfoPrint> SQLInterface::queryAllJobsPrint() {
	QVector<SQLNS::JobInfoPrint> jobs;
	if (!isConnected()) return jobs;
	QSqlDatabase db = databaseForCurrentThread();
	QSqlQuery q(db);
	if (!q.exec("SELECT jobId, jobName, companyId, recruitTypeId, cityId, sourceId, requirements, "
				"salaryMin, salaryMax, salarySlabId, createTime, updateTime, hrLastLoginTime "
				"FROM Job ORDER BY jobId ASC")) {
		qDebug() << "Select Job failed:" << q.lastError().text();
		return jobs;
	}
	while (q.next()) {
		SQLNS::JobInfoPrint job;
		job.jobId = q.value(0).toLongLong();
		job.jobName = q.value(1).toString();
		job.companyId = q.value(2).toInt();
		job.recruitTypeId = q.value(3).toInt();
		job.cityId = q.value(4).toInt();
		job.sourceId = q.value(5).toInt();
		job.requirements = q.value(6).toString();
		job.salaryMin = q.value(7).toDouble();
		job.salaryMax = q.value(8).toDouble();
		job.salarySlabId = q.value(9).toInt();
		job.createTime = q.value(10).toString();
		job.updateTime = q.value(11).toString();
		job.hrLastLoginTime = q.value(12).toString();

		// Resolve company name
		QSqlQuery qc(db);
		qc.prepare("SELECT companyName FROM Company WHERE companyId = :id");
		qc.bindValue(":id", job.companyId);
		if (qc.exec() && qc.next()) {
			job.companyName = qc.value(0).toString();
		}

		// Resolve recruit type name
		QSqlQuery qr(db);
		qr.prepare("SELECT typeName FROM RecruitType WHERE recruitTypeId = :id");
		qr.bindValue(":id", job.recruitTypeId);
		if (qr.exec() && qr.next()) {
			job.recruitTypeName = qr.value(0).toString();
		}

		// Resolve city name
		QSqlQuery qcity(db);
		qcity.prepare("SELECT cityName FROM JobCity WHERE cityId = :id");
		qcity.bindValue(":id", job.cityId);
		if (qcity.exec() && qcity.next()) {
			job.cityName = qcity.value(0).toString();
		}

		// Resolve source info
		QSqlQuery qs(db);
		qs.prepare("SELECT sourceName FROM Source WHERE sourceId = :id");
		qs.bindValue(":id", job.sourceId);
		if (qs.exec() && qs.next()) {
			job.sourceName = qs.value(0).toString();
		}

		// Query tags (IDs and names)
		QSqlQuery tagQuery(db);
		tagQuery.prepare("SELECT t.tagId, t.tagName FROM JobTag t "
						 "JOIN JobTagMapping m ON t.tagId = m.tagId "
						 "WHERE m.jobId = :jobId");
		tagQuery.bindValue(":jobId", job.jobId);
		if (tagQuery.exec()) {
			while (tagQuery.next()) {
				job.tagIds.append(tagQuery.value(0).toInt());
				job.tagNames.append(tagQuery.value(1).toString());
			}
		}

		jobs.append(job);
	}
	return jobs;
}

QSqlDatabase SQLInterface::databaseForCurrentThread() {
	QString connName = QStringLiteral("crawler_conn_%1").arg((quintptr)QThread::currentThreadId());
	if (QSqlDatabase::contains(connName)) {
		return QSqlDatabase::database(connName);
	}
	QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
	db.setDatabaseName(m_dbFilePath);
	if (!db.open()) {
		qDebug() << "Failed to open DB for thread:" << connName << db.lastError().text();
	}
	return db;
}