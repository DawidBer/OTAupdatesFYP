-- phpMyAdmin SQL Dump
-- version 5.2.1
-- https://www.phpmyadmin.net/
--
-- Host: 127.0.0.1
-- Generation Time: Mar 24, 2025 at 11:29 AM
-- Server version: 10.4.32-MariaDB
-- PHP Version: 8.2.12

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `cars`
--

-- --------------------------------------------------------

--
-- Table structure for table `carsandvin`
--

CREATE TABLE `carsandvin` (
  `id` int(11) NOT NULL,
  `vin` varchar(16) NOT NULL,
  `name` varchar(64) NOT NULL,
  `details` varchar(128) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `carsandvin`
--

INSERT INTO `carsandvin` (`id`, `vin`, `name`, `details`) VALUES
(15, '17', 'BMW/520d', 'fff');

-- --------------------------------------------------------

--
-- Table structure for table `filesandcars`
--

CREATE TABLE `filesandcars` (
  `id` int(11) NOT NULL,
  `version` varchar(32) NOT NULL,
  `carname` varchar(32) NOT NULL,
  `file` blob NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `filesandcars`
--

INSERT INTO `filesandcars` (`id`, `version`, `carname`, `file`) VALUES
(25, 'V21', 'BMW', 0x7468697320697320746865206f7468657220636f64652072756e6e696e67207269676874206e6f77203b290a3a31303030313030304646464646464646464646),
(26, 'V21', 'Audi', 0x68656c6c6f206576657279626f647920686f772061726520796f7520646f696e6720746f6461793f203a290a3a31303030313030304646464646464646464646),
(27, 'V1', 'MERCCAPS', 0x48456c6c6f206576657279626f647920686f772061726520796f7520646f696e6720746f6461793f203a290a3a31303030313030304646464646464646464646),
(28, 'V222', '128Byte', 0x7468697320697320746865206f7468657220636f64652072756e6e696e67207269676874206e6f77203b290a7468697320697320746865206f7468657220636f64652072756e6e696e67207269676874206e6f77203b290a3a313030303130303046464646464646464646464646464646464646464646464646464646464646);

--
-- Indexes for dumped tables
--

--
-- Indexes for table `carsandvin`
--
ALTER TABLE `carsandvin`
  ADD PRIMARY KEY (`id`),
  ADD UNIQUE KEY `vin` (`vin`);

--
-- Indexes for table `filesandcars`
--
ALTER TABLE `filesandcars`
  ADD PRIMARY KEY (`id`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `carsandvin`
--
ALTER TABLE `carsandvin`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=17;

--
-- AUTO_INCREMENT for table `filesandcars`
--
ALTER TABLE `filesandcars`
  MODIFY `id` int(11) NOT NULL AUTO_INCREMENT, AUTO_INCREMENT=29;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
