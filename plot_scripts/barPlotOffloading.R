#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)

args <- commandArgs(trailingOnly=TRUE)
file <- args[1] 
plot_filename <- args[2]
data<-read.csv(file)
data$Scheme <- factor(data$Scheme, levels=c("SP", "LR", "WF", "LND", "DCTCP_qdelay"), ordered=T)

#rel_schemes <- data[ which(data$Scheme=='DCTCP_qdelay'),]
label_list <- c("PS" = "Primal Dual Approach",
                 "SP" = "Shortest Path",
                 "LR" = "Landmark Routing",
                 "WF" = "Waterfilling", 
                 "LND" = "LND",
                 "DCTCP" = "Diffindo",
                 "DCTCP_qdelay" = "Spider")

break_list <- c("PS", "SP", "LR", "WF", "LND", "DCTCP", "DCTCP_qdelay")

color_list <- c(
                "SP" = "#e51a1c",
                "PS" = "#377eb8",
                "LR" = "#4daf4a",
                "WF" ="#984ea3",
                "LND" = "#fb9a99",
                "DCTCP" = "#a65628",
                "DCTCP_qdelay" = "#0c2c84")
pos <- "top"
height <- 3


# add a line plot
offloading_plot <- ggplot(data, aes_string(x="Topo", y="Offloading", fill="Scheme")) +
        geom_bar(stat="identity", position=position_dodge(), colour="black") +
        geom_errorbar(aes_string(ymin="OffloadingMin", ymax="OffloadingMax"), width=.3, 
                      position=position_dodge(.9)) +
        labs(x="Topology", y="Offchain tx/onchain tx") +
        scale_fill_manual(values=color_list, labels=label_list,breaks=break_list) +
        #theme_minimal(base_size=12) +
        theme(plot.margin=margin(c(0.5,0,0,0.5))) +
        theme(legend.title=element_blank()) +
        #theme(legend.text=element_text(size=rel(0.75)), 
        #      legend.key.size=unit(15,"points"),
        #      legend.position=pos, legend.margin=margin(c(0,5,1,5)))


ggsave(plot_filename, width=4, height=2)
