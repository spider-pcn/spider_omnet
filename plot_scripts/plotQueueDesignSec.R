#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)

args <- commandArgs(trailingOnly=TRUE)
file <- "linetopo_queueTimeSeriesData"
plot_filename <- "queueTimeSeries.pdf"
data<-read.csv(file)

label_list <- c("Q1" = expression("q"[AB]),
                 "Q2" = expression("q"[BA]),
                 "Q3" = expression("q"[BC]),
                 "Q4" = expression("q"[CB]))

break_list <- c("1", "2", "3", "4")

color_list <- c(
                "Q3" = "#00AFBB",
                "Q4" = "#00AFBB",
                "Q1" = "#FC4E07",
                "Q2" = "#FC4E07")

line_list <- c(
                "Q1" = "solid",
                "Q2" = "twodash",
                "Q3" = "solid",
                "Q4" = "twodash")

# add a line plot
queue_plot <- ggplot(data, aes_string(x="Time",y="Value", colour="QueueId", linetype="QueueId")) +
        geom_line(size=1) +
        scale_x_continuous(expand=c(0,0)) +
        coord_cartesian(xlim=c(0,15.5)) +
        labs(x="Time(s)", y="Queue Size") +
        scale_colour_manual(name="Guide",
                values=color_list,
                labels=label_list,
                guide=guide_legend(title=NULL, nrow=1)) +

        scale_linetype_manual(
                    values=line_list,
                    labels=label_list,
                    guide=guide_legend(title=NULL, nrow =1)) +

        theme_minimal(base_size=14) + 
        theme(legend.text=element_text(size=rel(0.8)), legend.key.size=unit(15,"points"),legend.position="top")

ggsave(plot_filename, width=4.3, height=3.2)
