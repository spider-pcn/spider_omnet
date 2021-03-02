#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot)

args <- commandArgs(trailingOnly=TRUE)
queue_filename <- 'lndDemand300QueueCDF.pdf'
inflight_filename <- 'lndDemand300InflightCDF.pdf'
queue_data<-read.csv('psQueueSize_demand300_data')
inflight_data<-read.csv('psInflight_demand300_data')

label_list <- c("PSWindow" = "Diffindo",
                 "PSNoWindow" = "Without Window",
                 "PSNoQueue" = "Without AQM",
                 "PSNoSplit" = "Without Packetization")

break_list <- c("PSWindow", "PSNoWindow", "PSNoQueue", "PSNoSplit")

color_list <- c(
                "PSWindow" = "#e51a1c",
                "PSNoWindow" = "#377eb8",
                "PSNoQueue" = "#4daf4a",
                "PSNoSplit" ="#984ea3")


# add a line plot
queue_plot <- ggplot(queue_data, aes_string(x="value",colour="scheme")) +
        stat_ecdf(size=1, geom='step') +
        scale_x_continuous(expand=c(0,0)) +
        #coord_cartesian(xlim=c(0,limit)) +
        labs(x="CDF", y="Queue Size (TU)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=2)) +

        theme_minimal(base_size=22) + 
        theme(legend.text=element_text(size=rel(0.7)), legend.key.size=unit(10,"points"), legend.position="top")

ggsave(queue_filename, width=5.7, height=5)


# add a line plot
inflight_plot <- ggplot(inflight_data, aes_string(x="value",colour="scheme")) +
        stat_ecdf(size=1, geom='step') +
        scale_x_continuous(expand=c(0,0)) +
        #coord_cartesian(xlim=c(0,limit)) +
        labs(x="CDF", y="Amount in flight (TU)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=2)) +

        theme_minimal(base_size=22) + 
        theme(legend.text=element_text(size=rel(0.7)), legend.key.size=unit(10,"points"), legend.position="top")

ggsave(inflight_filename, width=5.7, height=5)
