-- ============================================================================
--  图书馆借阅管理系统 — 数据库结构
--
--  用法（已有数据库时可直接执行，脚本是幂等的）：
--      mysql -u root -p < sql/schema.sql
--
--  说明：
--    * book.available 表示「当前可借数量」，由程序在借出/归还时维护，
--      约束为 0 <= available <= total。
-- ============================================================================

CREATE DATABASE IF NOT EXISTS `book_system`
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_0900_ai_ci;

USE `book_system`;

-- ---------------------------------------------------------------------------
-- 图书
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `book` (
    `id`        INT          NOT NULL AUTO_INCREMENT,
    `book_name` VARCHAR(50)  NOT NULL                       COMMENT '书名',
    `ISBN`      VARCHAR(13)  NOT NULL                       COMMENT '13 位数字，业务主键',
    `total`     INT          NOT NULL                       COMMENT '馆藏总量',
    `available` INT          NOT NULL DEFAULT 0             COMMENT '当前可借数量',
    `author`    VARCHAR(50)  NOT NULL                       COMMENT '作者',
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_ISBN` (`ISBN`),
    CONSTRAINT `chk_ISBN` CHECK (REGEXP_LIKE(`ISBN`, '^[0-9]{13}$')),
    CONSTRAINT `chk_total` CHECK (`total` >= 0),
    CONSTRAINT `chk_available` CHECK (`available` >= 0 AND `available` <= `total`)
) ENGINE = InnoDB
  DEFAULT CHARSET = utf8mb4
  COLLATE = utf8mb4_0900_ai_ci;

-- ---------------------------------------------------------------------------
-- 借阅记录
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `lend_records` (
    `id`             INT         NOT NULL AUTO_INCREMENT,
    `reader_id`      VARCHAR(20) NOT NULL                    COMMENT '借阅人',
    `isbn`           VARCHAR(20) NOT NULL                    COMMENT '所借图书的 ISBN',
    `borrow_time`    DATETIME    DEFAULT CURRENT_TIMESTAMP   COMMENT '借出时间',
    `borrow_count`   INT         NOT NULL DEFAULT 1          COMMENT '借出数量',
    `returned_count` INT         NOT NULL DEFAULT 0          COMMENT '累计已归还数量',
    PRIMARY KEY (`id`),
    KEY `idx_lend_isbn` (`isbn`),
    KEY `idx_lend_reader` (`reader_id`),
    CONSTRAINT `chk_lend_counts` CHECK (`returned_count` >= 0 AND `returned_count` <= `borrow_count`)
) ENGINE = InnoDB
  DEFAULT CHARSET = utf8mb4
  COLLATE = utf8mb4_0900_ai_ci;

-- ---------------------------------------------------------------------------
-- 归还记录
-- ---------------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS `return_records` (
    `id`           INT      NOT NULL AUTO_INCREMENT,
    `borrow_id`    INT      NOT NULL                  COMMENT '关联的借阅记录',
    `return_time`  DATETIME DEFAULT CURRENT_TIMESTAMP COMMENT '归还时间',
    `return_count` INT      NOT NULL                  COMMENT '本次归还数量',
    PRIMARY KEY (`id`),
    KEY `idx_return_borrow` (`borrow_id`),
    CONSTRAINT `fk_return_borrow`
        FOREIGN KEY (`borrow_id`) REFERENCES `lend_records` (`id`)
        ON DELETE CASCADE,
    CONSTRAINT `chk_return_count` CHECK (`return_count` > 0)
) ENGINE = InnoDB
  DEFAULT CHARSET = utf8mb4
  COLLATE = utf8mb4_0900_ai_ci;
